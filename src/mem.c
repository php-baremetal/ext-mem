/*
   +----------------------------------------------------------------------+
   | php-baremetal: ext-mem                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2026 Gianfrancesco Aurecchia                           |
   +----------------------------------------------------------------------+
   | This source file is subject to the MIT license, that is bundled with |
   | this package in the file LICENSE.                                    |
   +----------------------------------------------------------------------+
   | Authors: Gianfrancesco Aurecchia <gianfri@php-baremetal.com>         |
   +----------------------------------------------------------------------+
*/

/* mem_*: an in-RAM key-value store shared by every request of one boot, wiped on reset. */
#include "php.h"
#include "ext/standard/php_var.h"
#include "zend_smart_str.h"

/* key -> persistent zend_string with the serialized value; outlives every request. */
static HashTable s_mem;
static bool s_ready;

static void mem_val_dtor(zval *zv)
{
    zend_string *blob = Z_PTR_P(zv);
    if (blob) {
        zend_string_release_ex(blob, 1 /* persistent */);
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_clear, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_keys, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_available, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* mem_set(string $key, mixed $value): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_set, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()
PHP_FUNCTION(mem_set)
{
    char *key;
    size_t klen;
    zval *val;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(key, klen)
        Z_PARAM_ZVAL(val)
    ZEND_PARSE_PARAMETERS_END();

    if (!s_ready || klen == 0) {
        RETURN_FALSE;
    }

    smart_str buf = {0};
    php_serialize_data_t var_hash;
    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&buf, val, &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);
    if (!buf.s) {
        smart_str_free(&buf);
        RETURN_FALSE;
    }

    zend_string *blob = zend_string_init(ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), 1 /* persistent */);
    smart_str_free(&buf);

    zend_hash_str_update_ptr(&s_mem, key, klen, blob);
    RETURN_TRUE;
}

/* mem_get(string $key, mixed $default = null): mixed */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_get, 0, 1, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, default, IS_MIXED, 0, "null")
ZEND_END_ARG_INFO()
PHP_FUNCTION(mem_get)
{
    char *key;
    size_t klen;
    zval *def = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(key, klen)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(def)
    ZEND_PARSE_PARAMETERS_END();

    if (s_ready && klen > 0) {
        zend_string *blob = zend_hash_str_find_ptr(&s_mem, key, klen);
        if (blob) {
            const unsigned char *p = (const unsigned char *) ZSTR_VAL(blob);
            const unsigned char *end = p + ZSTR_LEN(blob);
            php_unserialize_data_t var_hash;
            PHP_VAR_UNSERIALIZE_INIT(var_hash);
            bool ok = php_var_unserialize(return_value, &p, end, &var_hash);
            PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
            if (ok) {
                return;
            }
            zval_ptr_dtor(return_value);   /* unreadable: fall back to the default */
            ZVAL_UNDEF(return_value);
        }
    }
    if (def) {
        RETURN_ZVAL(def, 1, 0);
    }
    RETURN_NULL();
}

/* mem_has(string $key): bool, also used by mem_delete */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_key, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()
PHP_FUNCTION(mem_has)
{
    char *key;
    size_t klen;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(key, klen)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(s_ready && klen > 0 && zend_hash_str_exists(&s_mem, key, klen));
}

/* mem_delete(string $key): bool */
PHP_FUNCTION(mem_delete)
{
    char *key;
    size_t klen;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(key, klen)
    ZEND_PARSE_PARAMETERS_END();

    if (!s_ready || klen == 0) {
        RETURN_FALSE;
    }
    RETURN_BOOL(zend_hash_str_del(&s_mem, key, klen) == SUCCESS);
}

/* mem_clear(): bool */
PHP_FUNCTION(mem_clear)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (!s_ready) {
        RETURN_FALSE;
    }
    zend_hash_clean(&s_mem);
    RETURN_TRUE;
}

/* mem_keys(): array */
PHP_FUNCTION(mem_keys)
{
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
    if (!s_ready) {
        return;
    }
    zend_string *key;
    ZEND_HASH_FOREACH_STR_KEY(&s_mem, key) {
        if (key) {
            /* copy: the stored key is persistent memory */
            add_next_index_stringl(return_value, ZSTR_VAL(key), ZSTR_LEN(key));
        }
    } ZEND_HASH_FOREACH_END();
}

/* mem_available(): bool */
PHP_FUNCTION(mem_available)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(s_ready);
}

PHP_MINIT_FUNCTION(mem)
{
    zend_hash_init(&s_mem, 8, NULL, mem_val_dtor, 1 /* persistent */);
    s_ready = true;
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mem)
{
    if (s_ready) {
        zend_hash_destroy(&s_mem);
        s_ready = false;
    }
    return SUCCESS;
}

static const zend_function_entry mem_functions[] = {
    PHP_FE(mem_set,       arginfo_mem_set)
    PHP_FE(mem_get,       arginfo_mem_get)
    PHP_FE(mem_has,       arginfo_mem_key)
    PHP_FE(mem_delete,    arginfo_mem_key)
    PHP_FE(mem_clear,     arginfo_mem_clear)
    PHP_FE(mem_keys,      arginfo_mem_keys)
    PHP_FE(mem_available, arginfo_mem_available)
    PHP_FE_END
};

zend_module_entry mem_module_entry = {
    STANDARD_MODULE_HEADER,
    "mem",
    mem_functions,
    PHP_MINIT(mem),
    PHP_MSHUTDOWN(mem),
    NULL, NULL, NULL,
    "1.0",
    STANDARD_MODULE_PROPERTIES,
};
