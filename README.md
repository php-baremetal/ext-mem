# ext-mem

An in-RAM key-value store for the PHP that runs on your board. `mem_set()` keeps any serializable PHP value under a
string key, and `mem_get()` returns a fresh copy of it. The table lives below PHP, so it is shared by every request
of one boot (the `web-server` runtime starts each request with empty variables) and wiped on reset. Nothing is
written to flash, so writing often costs no wear.

ext-mem is pure: it needs no HAL and works on every platform. Values that must survive a reset belong in
[ext-store](package:php-baremetal/ext-store).

## Install

In a project created with `phpbm init`:

```bash
composer require php-baremetal/ext-mem
```

## Example

```php
<?php
$hits = (int) mem_get('hits', 0) + 1;   // survives from one request to the next
mem_set('hits', $hits);
mem_set('last', ['at' => time(), 'path' => '/']);

echo "request #$hits, keys: ", implode(', ', mem_keys()), "\n";
```

## Documentation

- [Overview](docs/overview.md): requirements, install, a first project.
- [PHP API](docs/usage/php-api.md): the seven `mem_*` functions.
- [Values and lifetime](docs/usage/values-and-lifetime.md): what can be stored, copies, limits.
- Recipes: [share state across requests](docs/recipes/share-state-across-requests.md),
  [a bounded cache](docs/recipes/bounded-cache.md).

## License

MIT.
