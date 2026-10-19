---
eyebrow: 'Package · Extension'
lede:    'A key-value store in RAM for PHP on the board: any serializable value under a string key, shared by every request of one boot, wiped on reset. Pure extension, no HAL.'
see_also:
  - { href: './usage/php-api.md', meta: 'PHP API' }
  - { href: './usage/values-and-lifetime.md', meta: 'Reference' }
  - { href: 'package:php-baremetal/ext-store#docs/overview.md', meta: 'Extension', label: 'ext-store (persistent store)' }
  - { href: 'package:php-baremetal/ext-hello#docs/overview.md', meta: 'Extension', label: 'ext-hello' }
prev: { label: 'No previous page', href: '#' }
next: { label: 'PHP API', href: './usage/php-api.md' }
---

# In-RAM store

ext-mem adds seven global functions, `mem_set()`, `mem_get()`, `mem_has()`, `mem_delete()`, `mem_clear()`,
`mem_keys()` and `mem_available()`. The table sits in the firmware, below PHP, so what you store outlives the PHP
request that stored it. Two cases need that:

- the `web-server` runtime, where every HTTP request runs the script with fresh variables;
- a counter, a cache or a latest reading that several parts of the program read.

It is a copy store, like APCu: `mem_set()` serializes the value and keeps the bytes, `mem_get()` builds a new value
from them. Nothing survives a reset; for that use [ext-store](package:php-baremetal/ext-store#docs/overview.md).

## Requirements

Pure. ext-mem needs no HAL, so conductor adds nothing for it and it builds for whatever platform your project
targets. The only requirement is `php >=8.3`. The extension's PHP module is `mem`.

## Install

<!-- @steps -->
- **Install conductor once per machine**

  `composer global require php-baremetal/conductor` keeps the plugin active during the first solve of a new
  project.

- **Create the project**

  `phpbm init my-counter` writes `composer.json` with conductor (`^1.0`, allowed as a plugin), the package
  registry, and the SDK, board, runtime and storage you pick.

- **Require the extension**

  `composer require php-baremetal/ext-mem`

- **Build and flash**

  `phpbm flash` builds the firmware and flashes it; `phpbm monitor` opens the serial console.
<!-- @endsteps -->

The resulting `composer.json`, for a Waveshare ESP32-S3-Touch-LCD-1.85B serving HTTP over Wi-Fi, with a script that runs once at boot to
seed the table:

<!-- @code-block language="json" label="composer.json" -->
```json
{
  "name": "acme/my-counter",
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
      "sdk": "esp32@^1.0",
      "board": "waveshare-esp32-s3-touch-lcd-1.85b",
      "runtime": "web-server",
      "storage": "embedded",
      "options": { "init": "init.php" }
    }
  }
}
```
<!-- @endcode-block -->

## A first script

`project-src/index.php` runs once per HTTP request. A plain PHP variable would restart at 0 every time; the counter
in `mem_*` does not.

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
$hits = (int) mem_get('hits', 0) + 1;
mem_set('hits', $hits);

header('Content-Type: text/plain');
echo "request #$hits since boot\n";
```
<!-- @endcode-block -->

## PHP API at a glance

| Function | Returns | Meaning |
| --- | --- | --- |
| `mem_set(string $key, mixed $value)` | `bool` | Store a serialized copy. Empty key gives `false`. |
| `mem_get(string $key, mixed $default = null)` | `mixed` | A fresh copy, or `$default`. |
| `mem_has(string $key)` | `bool` | Is the key present. |
| `mem_delete(string $key)` | `bool` | `true` when the key existed. |
| `mem_clear()` | `bool` | Drop every key. |
| `mem_keys()` | `array` | The keys, in insertion order. |
| `mem_available()` | `bool` | `true` whenever the extension is built in. |

Details: [PHP API](./usage/php-api.md) and [Values and lifetime](./usage/values-and-lifetime.md).

## Notes

- **Wiped on reset.** A reset, power loss, crash or wake from deep sleep starts with an empty table. A
  `sys_light_sleep()` resumes the program where it stopped, so the table stays.
- **No flash.** Nothing touches flash, so there is no wear and no executor call; everything runs on the PHP task.
- **No limits but RAM.** No cap on key length, value size or entry count. A table that grows without bound ends in
  an out-of-memory failure of the whole program; see [Values and lifetime](./usage/values-and-lifetime.md).
- **No locking.** PHP runs one script at a time, so a read-modify-write inside one request cannot interleave with
  another.
