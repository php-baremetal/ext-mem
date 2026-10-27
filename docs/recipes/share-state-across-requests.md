---
eyebrow: 'ext-mem · Recipe'
lede:    'In the web-server runtime every HTTP request starts with empty PHP variables. Seed mem_* once from the init script, read and update it per request, and keep anything that must outlive a reset in store_*.'
see_also:
  - { href: '../usage/php-api.md', meta: 'PHP API' }
  - { href: '../usage/values-and-lifetime.md', meta: 'Reference' }
  - { href: './bounded-cache.md', meta: 'Recipe' }
  - { href: 'package:php-baremetal/ext-store#docs/overview.md', meta: 'Extension', label: 'ext-store (persistent store)' }
prev: { label: 'Values and lifetime', href: '../usage/values-and-lifetime.md' }
next: { label: 'A bounded cache', href: './bounded-cache.md' }
---

# Share state across requests

The `web-server` runtime is shared-nothing, like PHP-FPM: every HTTP request is a fresh run of your script, so a
variable assigned in one request is not there in the next. `mem_*` lives below PHP and is. This recipe seeds a value
once at boot and keeps a request counter in RAM.

## Goal

Every request shows a message computed once at boot and a counter of requests since boot.

## What you need

A board with a network connection (Wi-Fi from `init.php` with ext-wifi, or Ethernet) running a `web-server` project, and `php-baremetal/ext-mem`:

<!-- @code-block language="bash" label="terminal" -->
```bash
composer require php-baremetal/ext-mem
```
<!-- @endcode-block -->

The `web-server` runtime has an `init` option: a script, relative to `project-src`, that runs once after the engine
starts and before the HTTP server accepts its first connection.

<!-- @code-block language="json" label="composer.json" -->
```json
{
  "name": "acme/web-counter",
  "type": "project",
  "repositories": [
    { "type": "composer", "url": "https://repository.php-baremetal.com", "canonical": true }
  ],
  "require": {
    "php-baremetal/conductor": "^1.0",
    "php-baremetal/ext-mem": "^1.0"
  },
  "config": {
    "allow-plugins": { "php-baremetal/conductor": true },
    "platform": { "php": "8.5.11" }
  },
  "extra": {
    "baremetal": {
      "sdk": "esp32@^2.0",
      "board": "waveshare-esp32-s3-touch-lcd-1.85b",
      "runtime": "web-server",
      "storage": "embedded",
      "options": { "init": "init.php" }
    }
  }
}
```
<!-- @endcode-block -->

## The code

`init.php` is a request like any other: its variables vanish when it ends, but what it writes to `mem_*` stays. Its
output goes to the serial console, not to an HTTP response.

<!-- @code-block language="php" label="project-src/init.php: runs once at boot" -->
```php
<?php
echo "[init] seeding mem_*\n";
mem_set('boot_msg', 'initialised once at boot');
mem_set('hits', 0);
```
<!-- @endcode-block -->

<!-- @code-block language="php" label="project-src/index.php: runs per request" -->
```php
<?php
$hits = (int) mem_get('hits', 0) + 1;
mem_set('hits', $hits);

header('Content-Type: text/plain');
echo mem_get('boot_msg', '(unset)'), "\n";
echo "request #$hits since boot\n";
echo "mem keys: ", implode(', ', mem_keys()), "\n";
```
<!-- @endcode-block -->

<!-- @callout variant="tip" title="Read-modify-write is safe here" -->
`mem_get()`, change, `mem_set()` cannot race with another request: the web server runs PHP requests one at a time.
<!-- @endcallout -->

## What you will see

At boot the console shows the init script running once, before the server comes up. Then each request reads the
seeded message and bumps the counter:

<!-- @code-block language="bash" label="terminal: two requests" -->
```bash
curl http://BOARD_IP/
# initialised once at boot
# request #1 since boot
# mem keys: boot_msg, hits

curl http://BOARD_IP/
# initialised once at boot
# request #2 since boot
# mem keys: boot_msg, hits
```
<!-- @endcode-block -->

Reset the board and the count restarts from 1.

## Persistent settings, volatile copies

To serve a setting that must survive a reset, keep it in `store_*` and copy it into `mem_*` at boot. Requests then
read RAM only, and flash is touched once per boot. This needs `php-baremetal/ext-store` as well.

<!-- @code-block language="php" label="project-src/init.php: load settings once" -->
```php
<?php
mem_set('settings', [
    'name' => store_get('name', 'device'),
    'unit' => store_get('unit', 'C'),
]);
```
<!-- @endcode-block -->

See [ext-store](package:php-baremetal/ext-store#docs/overview.md).

<!-- @callout variant="note" title="Handles stay out" -->
Objects created in `init.php` are gone once it ends, and a live handle (a socket, an initialised display) cannot be
serialized into `mem_*`: a resource is stored as `0`. Bring hardware up once from `init.php` through its
extension's functions, and share only plain data through `mem_*`.
<!-- @endcallout -->
