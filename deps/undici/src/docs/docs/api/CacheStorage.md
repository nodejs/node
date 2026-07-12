# CacheStorage

<!--introduced_in=v5.22.1-->
<!--type=module-->
<!-- source_link=lib/web/cache/cachestorage.js -->

> Stability: 2 - Stable

undici provides a spec-compliant implementation of the [W3C Service Worker
`CacheStorage`][] and [`Cache`][] interfaces. A `CacheStorage` is a named
collection of [`Cache`][] objects, each of which stores `Request`/`Response`
pairs.

The class itself is not exported; instead, undici exposes a single shared
`CacheStorage` instance named `caches`. Import it from `'undici'`:

```mjs
import { caches } from 'undici'

const cache = await caches.open('v1')
```

```cjs
const { caches } = require('undici')

caches.open('v1').then((cache) => {
  // ...
})
```

## `caches`

<!-- YAML
added: v5.22.1
-->

* Type: {CacheStorage}

The shared [`CacheStorage`](#class-cachestorage) instance exported by undici.
All caches opened through `caches` live for the lifetime of the process.

```mjs
import { caches } from 'undici'

await caches.open('v1')
console.log(await caches.keys()) // ['v1']
```

## Class: `CacheStorage`

<!-- YAML
added: v5.22.1
-->

Represents the storage for [`Cache`][] objects. Each `CacheStorage` keeps a
mapping of cache names to their backing list of cached responses.

The constructor is not public: attempting to instantiate `CacheStorage`
directly throws a `TypeError`. Use the exported [`caches`](#caches) instance
instead.

### `caches.match(request[, options])`

<!-- YAML
added: v5.22.1
-->

* `request` {Request|string} The request to match against. A string is treated
  as a URL.
* `options` {Object} (optional)
  * `ignoreSearch` {boolean} When `true`, ignores the query string of the URL
    when matching. **Default:** `false`.
  * `ignoreMethod` {boolean} When `true`, prevents matching operations from
    validating the `Request` `http` method. **Default:** `false`.
  * `ignoreVary` {boolean} When `true`, ignores the `Vary` header when matching.
    **Default:** `false`.
  * `cacheName` {string} When set, restricts the search to the cache with this
    name. **Default:** `undefined`.
* Returns: {Promise} Fulfills with the first matching {Response}, or
  `undefined` if no response matched.

Searches the caches for a response matching `request`. When `options.cacheName`
is provided, only that cache is searched; otherwise every cache is searched in
insertion order and the first match is returned.

```mjs
import { caches } from 'undici'

const response = await caches.match('https://example.com/data', {
  cacheName: 'v1'
})
```

### `caches.has(cacheName)`

<!-- YAML
added: v5.22.1
-->

* `cacheName` {string} The name of the cache to look up.
* Returns: {Promise} Fulfills with `true` if a cache named `cacheName` exists,
  otherwise `false`.

Checks whether a cache with the given name exists in the storage.

```mjs
import { caches } from 'undici'

await caches.open('v1')
console.log(await caches.has('v1')) // true
console.log(await caches.has('v2')) // false
```

### `caches.open(cacheName)`

<!-- YAML
added: v5.22.1
-->

* `cacheName` {string} The name of the cache to open.
* Returns: {Promise} Fulfills with the {Cache} named `cacheName`.

Opens the cache named `cacheName`, creating it if it does not already exist.

Each call returns a new [`Cache`][] object, but caches opened with the same name
share their underlying list of cached responses. As a result, a response stored
through one instance is visible through another instance with the same name.

```mjs
import { caches } from 'undici'
import assert from 'node:assert'

const cache1 = await caches.open('v1')
const cache2 = await caches.open('v1')

// open() returns distinct objects,
assert(cache1 !== cache2)
// but they share the same cached responses.
assert.deepStrictEqual(await cache1.match('/req'), await cache2.match('/req'))
```

### `caches.delete(cacheName)`

<!-- YAML
added: v5.22.1
-->

* `cacheName` {string} The name of the cache to delete.
* Returns: {Promise} Fulfills with `true` if the cache existed and was deleted,
  otherwise `false`.

Removes the cache named `cacheName` from the storage. Any [`Cache`][] objects
already obtained for that name, along with the `Request`/`Response` pairs they
returned, remain usable after deletion.

```mjs
import { caches } from 'undici'

const cache = await caches.open('v1')
const response = await cache.match('/req')

await caches.delete('v1')

// The already-retrieved response is still usable.
await response.text()
```

### `caches.keys()`

<!-- YAML
added: v5.22.1
-->

* Returns: {Promise} Fulfills with a {string[]} of cache names in insertion
  order.

Returns the names of all caches currently held by the storage.

```mjs
import { caches } from 'undici'

await caches.open('v1')
await caches.open('v2')
console.log(await caches.keys()) // ['v1', 'v2']
```

[W3C Service Worker `CacheStorage`]: https://w3c.github.io/ServiceWorker/#cachestorage-interface
[`Cache`]: https://w3c.github.io/ServiceWorker/#cache-interface
