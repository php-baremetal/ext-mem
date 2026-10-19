---
eyebrow: 'ext-mem · Reference'
lede:    'What mem_* keeps and for how long: serialized copies, never live objects; kept across every request of one boot; gone on reset. Anything serialize() cannot handle does not belong here.'
see_also:
  - { href: './php-api.md', meta: 'PHP API' }
  - { href: '../recipes/bounded-cache.md', meta: 'Recipe' }
  - { href: 'package:php-baremetal/ext-store#docs/overview.md', meta: 'Extension', label: 'ext-store (persistent store)' }
prev: { label: 'PHP API', href: './php-api.md' }
next: { label: 'Share state across requests', href: '../recipes/share-state-across-requests.md' }
---

# Values and lifetime

## Lifetime

| Event | What happens to `mem_*` |
| --- | --- |
| End of a PHP request | Kept. Your variables are gone, `mem_*` values are not. |
| Next HTTP request (`web-server` runtime) | Sees every value written by earlier requests and by the init script. |
| Loop tick (`init-loop` runtime) | Kept, like any variable, and shared with every function. |
| Light sleep | Kept: the program resumes where it stopped. |
| Reset, power loss, crash, wake from deep sleep | Gone. The table starts empty on the next boot. |

Nothing is written to flash, so nothing is restored after a reset, and writing on every request costs no wear. If a
value must survive a reset, keep it in `store_*` ([ext-store](package:php-baremetal/ext-store#docs/overview.md)); a
common pattern is to read `store_*` once at boot and copy what requests need into `mem_*`.

## Copies, not references

`mem_set()` runs the value through PHP's serializer and keeps the bytes. `mem_get()` rebuilds a new value from
those bytes for the caller. The model is APCu's, not a shared object:

- Every read is independent. Two reads of the same key give two separate arrays or objects.
- Changing a value means read, modify, `mem_set()` back. There is no in-place update.
- Object identity is not kept across calls; references inside one stored value are kept, as with `serialize()`.
- A large value costs its serialized size in RAM once, plus its rebuilt size in every request that reads it.

<!-- @callout variant="warning" title="Each mem_get is a copy" -->
`mem_get('cart')['items'][] = $item;` changes nothing in the store. Read into a variable, modify it, then
`mem_set('cart', $cart)`.
<!-- @endcallout -->

## What can be stored

| Value | Result |
| --- | --- |
| `null`, `bool`, `int`, `float`, `string`, arrays of these | Round-trips exactly. |
| Objects of serializable classes | Round-trips; `__serialize` and `__sleep`, `__unserialize` and `__wakeup` run. The class must be defined or autoloadable when read, or you get `__PHP_Incomplete_Class`. |
| `Closure`, `Generator`, other non-serializable objects | `mem_set()` throws an `Exception`. |
| Resources (`fopen()` streams, sockets) | Stored as the integer `0`. No error; the handle is lost. |
| Enums | Round-trip as the same case, if the enum is defined when read. |

The dividing line is data versus live handle. If you can `serialize()` it, it can live in `mem_*`. A live handle (an
open socket, an initialised display, a bus) cannot survive a request teardown at all; bring it up once, through its
extension's functions, and share only plain data through `mem_*`.

## Limits

| Limit | Value |
| --- | --- |
| Key length | no cap; the empty string is rejected |
| Value size | no cap besides free RAM |
| Number of keys | no cap besides free RAM |
| Quota | none |

Every stored byte comes out of the RAM the PHP engine and the rest of the firmware use, and running out of it is
fatal: `mem_set()` does not return `false` for a full heap. Count keys, bound caches, and `mem_delete()` what you no
longer need. Keys derived from request input (a client IP, a path, a query string) grow without limit unless you
prune them; see [A bounded cache](../recipes/bounded-cache.md).

## Concurrency

There is no locking, and none is needed: PHP runs one request at a time on the device. A read-modify-write
(`mem_get()`, change, `mem_set()`) inside one request cannot interleave with another request.
