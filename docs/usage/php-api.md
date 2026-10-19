---
eyebrow: 'ext-mem · PHP API'
lede:    'The seven mem_* functions with their real signatures. All work on one table shared by every request; each value goes in and comes out through PHP serialization, so every read is an independent copy.'
see_also:
  - { href: './values-and-lifetime.md', meta: 'Reference' }
  - { href: '../recipes/share-state-across-requests.md', meta: 'Recipe' }
  - { href: '../recipes/bounded-cache.md', meta: 'Recipe' }
  - { href: 'package:php-baremetal/ext-store#docs/overview.md', meta: 'Extension', label: 'ext-store (persistent store)' }
prev: { label: 'Overview', href: '../overview.md' }
next: { label: 'Values and lifetime', href: './values-and-lifetime.md' }
---

# PHP API

Seven global functions. Keys are any non-empty string: no length cap, no character restriction, binary-safe. The
empty string is rejected by every key-taking function. None of them raise an error for a missing key or an empty
table; wrong argument types and counts raise the usual `TypeError` and `ArgumentCountError`.

| Function | Returns | Notes |
| --- | --- | --- |
| `mem_set(string $key, mixed $value)` | `bool` | Store a serialized copy. Empty key gives `false`. |
| `mem_get(string $key, mixed $default = null)` | `mixed` | A fresh copy, or `$default` if absent, empty key or unreadable. |
| `mem_has(string $key)` | `bool` | Is the key present. Empty key gives `false`. |
| `mem_delete(string $key)` | `bool` | `true` if the key existed and was removed. |
| `mem_clear()` | `bool` | Drop every key; `true`. |
| `mem_keys()` | `array` | List of stored keys (strings), in insertion order. |
| `mem_available()` | `bool` | `true` whenever the extension is built in. |

## mem_set

<!-- @method name="mem_set(string $key, mixed $value): bool" returns="bool" visibility="public" -->

Serializes `$value` and stores it under `$key`, replacing any previous value. Returns `true` on success, `false`
for an empty key or a value that serialized to nothing. What is stored is a snapshot: changing the variable
afterwards does not change the store.

<!-- @params -->
<!-- @param name="$key" type="string" required -->
Non-empty key. An `int` passed here is coerced to its string form unless `strict_types` is on.
<!-- @endparam -->
<!-- @param name="$value" type="mixed" required -->
Any value `serialize()` accepts: `null`, scalars, arrays, objects of serializable classes.
<!-- @endparam -->
<!-- @endparams -->

<!-- @callout variant="warning" title="Values that do not round-trip" -->
A `Closure`, a `Generator` or any other object whose class forbids serialization makes `mem_set()` throw an
`Exception` ("Serialization of 'Closure' is not allowed"). A resource (a stream, a socket) does not throw: it is
stored as the integer `0`, so the handle is silently lost.
<!-- @endcallout -->

## mem_get

<!-- @method name="mem_get(string $key, mixed $default = null): mixed" returns="mixed" visibility="public" -->

Rebuilds the value stored under `$key` as a new, independent PHP value and returns it. Returns `$default` when the
key is absent, when `$key` is empty, or when the stored bytes cannot be unserialized. Each call builds a new copy;
changing it does not touch the store until you `mem_set()` it back.

<!-- @params -->
<!-- @param name="$key" type="string" required -->
The key to read.
<!-- @endparam -->
<!-- @param name="$default" type="mixed" default="null" -->
Returned when there is nothing readable under the key.
<!-- @endparam -->
<!-- @endparams -->

A stored `null` and a missing key both give `null` with the default `$default`; use `mem_has()` to tell them
apart. The same goes for a stored `false` and a `false` default. Objects are rebuilt with the normal unserialize
rules (`__unserialize` and `__wakeup` run); the class must be defined or autoloadable in the reading request,
otherwise you get a `__PHP_Incomplete_Class`.

## mem_has

<!-- @method name="mem_has(string $key): bool" returns="bool" visibility="public" -->

`true` if `$key` is in the table, `false` otherwise or for an empty key. It does not read the value, so it is
`true` even for a stored value `mem_get()` cannot rebuild.

## mem_delete

<!-- @method name="mem_delete(string $key): bool" returns="bool" visibility="public" -->

Removes `$key` and frees its memory. Returns `true` if the key existed and was removed, `false` if it did not
exist or the key is empty.

## mem_clear

<!-- @method name="mem_clear(): bool" returns="bool" visibility="public" -->

Removes every key and frees every value. Takes no arguments. Returns `true`.

## mem_keys

<!-- @method name="mem_keys(): array" returns="array" visibility="public" -->

Returns a list (`0` to `n-1`) of the keys currently stored, as strings, in insertion order. Overwriting a key keeps
its position; deleting and setting it again moves it to the end. Numeric-looking keys stay strings (`'42'`, not
`42`). Returns `[]` when the table is empty.

## mem_available

<!-- @method name="mem_available(): bool" returns="bool" visibility="public" -->

Returns `true`. There is nothing to configure or mount, so unlike `store_available()` it never reports a disabled
store. Its practical use is behind a `function_exists('mem_available')` check, to detect a firmware built with
ext-mem.

## Example

<!-- @code-block language="php" label="project-src/index.php: the whole API" -->
```php
<?php
mem_set('greeting', 'hello');
mem_set('cfg', ['unit' => 'C', 'every' => 5]);
mem_set('nothing', null);

var_dump(mem_get('cfg')['unit']);        // string(1) "C"
var_dump(mem_get('missing', 'n/a'));     // string(3) "n/a"
var_dump(mem_has('nothing'));            // bool(true): a stored null
var_dump(mem_keys());                    // ['greeting', 'cfg', 'nothing']

$cfg = mem_get('cfg');
$cfg['every'] = 10;                      // a copy: the store is unchanged
mem_set('cfg', $cfg);                    // publish the change

var_dump(mem_delete('greeting'));        // bool(true)
var_dump(mem_delete('greeting'));        // bool(false): already gone
mem_clear();
var_dump(mem_keys());                    // array(0) {}
```
<!-- @endcode-block -->
