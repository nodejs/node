# Cache Store

<!--introduced_in=v7.0.0-->
<!--type=module-->
<!-- source_link=lib/cache/memory-cache-store.js -->

> Stability: 2 - Stable

A cache store is the storage backend used by the cache interceptor to persist
and retrieve cached responses. A store decides which stored response to serve
for a request by comparing the request against the response's `Vary` header, and
is expected to be compliant with [RFC&nbsp;9111][].

undici ships two stores. `MemoryCacheStore` keeps responses in memory, while
`SqliteCacheStore` persists them to a SQLite database. Both are available under
the `cacheStores` export:

```mjs
import { cacheStores } from 'undici'

const { MemoryCacheStore, SqliteCacheStore } = cacheStores
```

`SqliteCacheStore` requires the [`node:sqlite`][] API. The class is always
exported, but constructing one throws if `node:sqlite` is not available in the
running Node.js version.

A cache store implements three methods — `get(key)`, `createWriteStream(key,
value)`, and `delete(key)` — that operate on a [`CacheKey`](#cachekey) and a
[`CacheValue`](#cachevalue). Any object implementing this contract can be passed
to the cache interceptor, so custom stores (for example, backed by Redis or a
remote service) are supported as well.

## Class: `MemoryCacheStore`

<!-- YAML
added: v7.0.0
-->

* Extends: {EventEmitter}

Stores cached responses in memory. The store enforces upper bounds on the total
number of responses, the total size of all responses, and the size of any single
response. When a limit is exceeded, the store evicts approximately half of its
entries and emits a [`'maxSizeExceeded'`](#event-maxsizeexceeded) event.

```mjs
import { interceptors, cacheStores, Agent, setGlobalDispatcher } from 'undici'

const store = new cacheStores.MemoryCacheStore({ maxSize: 50 * 1024 * 1024 })

setGlobalDispatcher(
  new Agent().compose(interceptors.cache({ store }))
)
```

### `new MemoryCacheStore([options])`

<!-- YAML
added: v7.0.0
-->

* `options` {Object} (optional)
  * `maxCount` {number} The maximum number of responses to store. **Default:**
    `1024`.
  * `maxSize` {number} The maximum total size, in bytes, of all stored
    responses. **Default:** `104857600` (100 MiB).
  * `maxEntrySize` {number} The maximum size, in bytes, of a single response
    body. Responses whose body exceeds this value are not cached. **Default:**
    `5242880` (5 MiB).
  * `errorCallback` {Function} A callback invoked with any error raised by the
    store. (optional)
    * `err` {Error}

Each of `maxCount`, `maxSize`, and `maxEntrySize` must be a non-negative
integer; a `TypeError` is thrown otherwise.

### `memoryCacheStore.size`

<!-- YAML
added: v7.0.0
-->

* Type: {number}

The current total size, in bytes, of all stored response bodies.

### `memoryCacheStore.isFull()`

<!-- YAML
added: v7.0.0
-->

* Returns: {boolean} `true` when the store has reached its `maxSize` or
  `maxCount` limit, `false` otherwise.

### `memoryCacheStore.get(key)`

<!-- YAML
added: v7.0.0
-->

* `key` {CacheKey} The request to look up. See [`CacheKey`](#cachekey).
* Returns: {GetResult|undefined} The matching cached response, or `undefined`
  when there is no fresh entry for `key`. See [`GetResult`](#getresult).

Looks up a cached response for `key`. A stored entry matches only when its
`method` equals `key.method`, its `deleteAt` time is in the future, and every
header listed in its `vary` map matches the corresponding header in
`key.headers`.

### `memoryCacheStore.createWriteStream(key, value)`

<!-- YAML
added: v7.0.0
-->

* `key` {CacheKey} The request the response is being cached for. See
  [`CacheKey`](#cachekey).
* `value` {CacheValue} The response metadata to store. See
  [`CacheValue`](#cachevalue).
* Returns: {Writable|undefined} A writable stream that the response body is
  written to, or `undefined` when the response cannot be cached.

Returns a {Writable} stream used to write the response body into the store. When
the stream finishes, the entry is committed; if its accumulated size exceeds
`maxEntrySize`, the stream is destroyed and nothing is stored. Writing a new
entry whose `key` matches an existing one replaces that entry.

### `memoryCacheStore.delete(key)`

<!-- YAML
added: v7.0.0
-->

* `key` {CacheKey} The request whose cached responses should be removed. See
  [`CacheKey`](#cachekey).
* Returns: {undefined}

Removes every cached response stored for `key.origin` and `key.path`. Throws a
`TypeError` if `key` is not an object.

### Event: `'maxSizeExceeded'`

<!-- YAML
added: v7.10.0
-->

* `data` {Object}
  * `size` {number} The current total size, in bytes, of all stored responses.
  * `maxSize` {number} The configured `maxSize` limit.
  * `count` {number} The current number of stored responses.
  * `maxCount` {number} The configured `maxCount` limit.

Emitted when the store exceeds its `maxSize` or `maxCount` limit, immediately
before entries are evicted. The event fires only once per overflow; it is not
emitted again until the store drops back below both limits.

## Class: `SqliteCacheStore`

<!-- YAML
added: v7.0.0
-->

Stores cached responses in a SQLite database using the [`node:sqlite`][] API.
The constructor throws when [`node:sqlite`][] is not available.

```mjs
import { interceptors, cacheStores, Agent, setGlobalDispatcher } from 'undici'

const store = new cacheStores.SqliteCacheStore({ location: './cache.db' })

setGlobalDispatcher(
  new Agent().compose(interceptors.cache({ store }))
)
```

### `new SqliteCacheStore([options])`

<!-- YAML
added: v7.0.0
-->

* `options` {Object} (optional)
  * `location` {string} The location of the SQLite database. Use `':memory:'`
    for an in-memory database. **Default:** `':memory:'`.
  * `maxCount` {number} The maximum number of responses to store. **Default:**
    `Infinity`.
  * `maxEntrySize` {number} The maximum size, in bytes, of a single response
    body. Responses whose body exceeds this value are not cached. Must not
    exceed `2000000000` (2 GB). **Default:** `2000000000` (2 GB).

`maxCount` and `maxEntrySize` must be non-negative integers; a `TypeError` is
thrown otherwise, or if `maxEntrySize` is greater than 2 GB.

### `sqliteCacheStore.close()`

<!-- YAML
added: v7.0.0
-->

* Returns: {undefined}

Closes the connection to the underlying SQLite database.

### `sqliteCacheStore.size`

<!-- YAML
added: v7.0.0
-->

* Type: {number}

The number of responses currently stored in the database.

### `sqliteCacheStore.get(key)`

<!-- YAML
added: v7.0.0
-->

* `key` {CacheKey} The request to look up. See [`CacheKey`](#cachekey).
* Returns: {GetResult|undefined} The matching cached response, or `undefined`
  when there is no fresh entry for `key`. The returned `body` is a {Buffer}.
  See [`GetResult`](#getresult).

Looks up a cached response for `key`, comparing the request method and every
header named in the stored `vary` map.

### `sqliteCacheStore.set(key, value)`

<!-- YAML
added: v7.0.0
-->

* `key` {CacheKey} The request the response is being cached for. See
  [`CacheKey`](#cachekey).
* `value` {CacheValue} The response to store, with its `body` set to a {Buffer},
  an {Array} of {Buffer}, or `null`. See [`CacheValue`](#cachevalue).
* Returns: {undefined}

Writes a response into the database directly, without going through a stream. If
the body exceeds `maxEntrySize`, nothing is stored. When an entry already exists
for `key` it is overwritten; otherwise a new row is inserted and old entries are
pruned to honour `maxCount`. This method is used internally by
[`sqliteCacheStore.createWriteStream()`](#sqlitecachestorecreatewritestreamkey-value).

### `sqliteCacheStore.createWriteStream(key, value)`

<!-- YAML
added: v7.0.0
-->

* `key` {CacheKey} The request the response is being cached for. See
  [`CacheKey`](#cachekey).
* `value` {CacheValue} The response metadata to store. See
  [`CacheValue`](#cachevalue).
* Returns: {Writable|undefined} A writable stream that the response body is
  written to, or `undefined` when the response cannot be cached.

Returns a {Writable} stream used to write the response body into the store. When
the stream finishes, the buffered body is committed via
[`sqliteCacheStore.set()`](#sqlitecachestoresetkey-value). If the body exceeds
`maxEntrySize`, the stream is destroyed and nothing is stored.

### `sqliteCacheStore.delete(key)`

<!-- YAML
added: v7.0.0
-->

* `key` {CacheKey} The request whose cached responses should be removed. See
  [`CacheKey`](#cachekey).
* Returns: {undefined}

Removes every cached response stored for the URL derived from `key.origin` and
`key.path`. Throws a `TypeError` if `key` is not an object.

## Implementing a custom cache store

A cache store is any object that implements the `CacheStore` interface:

* `get(key)` — Look up a response. Returns a [`GetResult`](#getresult),
  `undefined`, or a {Promise} resolving to either.
* `createWriteStream(key, value)` — Return a {Writable} to receive the response
  body, or `undefined` if the response cannot be stored.
* `delete(key)` — Remove all responses for `key`. May return a {Promise}.

Returning a {Promise} from `get` or `delete` lets a store be backed by an
asynchronous resource; the cache interceptor awaits these values, including on
the revalidation and stale-while-revalidate paths.

### `CacheKey`

The request a response is being looked up or stored for.

* `origin` {string} The request origin.
* `method` {string} The request method.
* `path` {string} The request path.
* `headers` {Record<string, string|string[]>} The request headers, used to
  satisfy `Vary`. (optional)

### `CacheValue`

The metadata of a cached response, excluding its body.

* `statusCode` {number} The response's HTTP status code.
* `statusMessage` {string} The response's HTTP status message.
* `headers` {Record<string, string|string[]>} The response headers.
* `vary` {Record<string, string|string[]|null>} The header names listed in the
  response's `Vary` header mapped to the values they had in the original
  request. A value is `null` when that header was absent from the original
  request. (optional)
* `etag` {string} The response's entity tag. (optional)
* `cacheControlDirectives` {Object} The parsed `Cache-Control` directives of the
  response. (optional)
* `cachedAt` {number} The time, in milliseconds, at which the response was
  cached.
* `staleAt` {number} The time, in milliseconds, at which the response becomes
  stale.
* `deleteAt` {number} The time, in milliseconds, at which the response must be
  evicted. A store must not return a response once this time has passed.

The `vary` map drives response selection. For a response such as:

```http
Vary: content-encoding, accept
content-encoding: utf8
accept: application/json
```

the recorded map is:

```js
{
  'content-encoding': 'utf8',
  accept: 'application/json'
}
```

If the original request did not include the `accept` header, its value is
recorded as `null`:

```js
{
  'content-encoding': 'utf8',
  accept: null
}
```

### `GetResult`

The value returned by `get`. It contains every field of
[`CacheValue`](#cachevalue) plus the response body.

* `body` {Readable|Iterable|AsyncIterable|Buffer|string} The cached response
  body. (optional)

[RFC&nbsp;9111]: https://www.rfc-editor.org/rfc/rfc9111.html
[`node:sqlite`]: https://nodejs.org/api/sqlite.html
