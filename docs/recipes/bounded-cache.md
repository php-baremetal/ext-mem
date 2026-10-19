---
eyebrow: 'ext-mem · Recipe'
lede:    'Cache an expensive value in mem_* with a time-to-live, and cap the number of entries so keys derived from request input cannot eat the RAM.'
see_also:
  - { href: '../usage/php-api.md', meta: 'PHP API' }
  - { href: '../usage/values-and-lifetime.md', meta: 'Reference' }
  - { href: './share-state-across-requests.md', meta: 'Recipe' }
prev: { label: 'Share state across requests', href: './share-state-across-requests.md' }
next: { label: 'No next page', href: '#' }
---

# A bounded cache

`mem_*` has no quota and no expiry: whatever you store stays until you delete it or the board resets, and it all
comes out of the RAM PHP runs in. A cache on top of it needs two bounds: how long an entry is valid, and how many
entries there can be.

## Goal

Compute a report at most once a minute, and cache per-path responses with a hard cap of 32 entries, evicting the
oldest first.

## What you need

A `web-server` project, as in [Share state across requests](./share-state-across-requests.md), with
`php-baremetal/ext-mem`. The cache works the same in any runtime; it matters most where requests share nothing.

## The code

Entries live under a `cache:` prefix, each an array holding the value and its expiry time. `mem_keys()` returns keys
in insertion order, and deleting a key before setting it again moves it to the end, so the first `cache:` key in the
list is always the one written longest ago.

<!-- @code-block language="php" label="project-src/cache.php" -->
```php
<?php
const CACHE_PREFIX = 'cache:';
const CACHE_MAX    = 32;

function cache_get(string $key, int $ttl, callable $compute): mixed
{
    $entry = mem_get(CACHE_PREFIX . $key);
    if (is_array($entry) && $entry['until'] > time()) {
        return $entry['value'];
    }

    $value = $compute();
    cache_put($key, $value, $ttl);
    return $value;
}

function cache_put(string $key, mixed $value, int $ttl): void
{
    mem_delete(CACHE_PREFIX . $key);          // re-insert at the end of mem_keys()

    $cached = array_values(array_filter(
        mem_keys(),
        fn (string $k) => str_starts_with($k, CACHE_PREFIX),
    ));
    while (count($cached) >= CACHE_MAX) {     // evict the oldest entries
        mem_delete(array_shift($cached));
    }

    mem_set(CACHE_PREFIX . $key, ['until' => time() + $ttl, 'value' => $value]);
}
```
<!-- @endcode-block -->

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
require __DIR__ . '/cache.php';

$report = cache_get('report', 60, fn () => build_report());

$path = parse_url($_SERVER['REQUEST_URI'] ?? '/', PHP_URL_PATH);
$page = cache_get("page:$path", 300, fn () => render_page($path));

header('Content-Type: text/html');
echo $page;
```
<!-- @endcode-block -->

`build_report()` and `render_page()` stand for your own expensive work. Without `CACHE_MAX`, every distinct path a
client requests would add a key that is never removed.

## Notes

<!-- @callout variant="warning" title="Cache data, not handles" -->
The cached value goes through `serialize()`. Arrays, strings and serializable objects are fine. A closure throws; a
resource is stored as `0`. Cache the result of the work, not the thing that does it.
<!-- @endcallout -->

- **Count the bytes, not just the keys.** `CACHE_MAX` caps entries; a few large rendered pages can still use a lot
  of RAM. Running out is fatal, so size `CACHE_MAX` against your biggest value.
- **Clock jumps.** Expiry uses `time()`. If the wall clock is set while the board runs, for example by a network
  time sync, entries can expire early or live longer once. The cache is wiped on reset anyway.
- **Expired entries are not swept.** They are replaced when read again or evicted by the cap. To drop everything at
  once, delete the `cache:` keys, or `mem_clear()` if nothing else lives in `mem_*`.
- **Need it after a reset?** A cache rarely does. Settings that must survive belong in `store_*`
  ([ext-store](package:php-baremetal/ext-store#docs/overview.md)).
