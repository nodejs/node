# KV store

<!--introduced_in=v27.0.0-->

<!-- YAML
added: v27.0.0
-->

> Stability: 1 - Experimental.

<!-- source_link=lib/kvstore.js -->

The `node:kvstore` module facilitates working with a KV store database.
To access it:

```mjs
import { openKv } from 'node:kvstore';
```

```cjs
const { openKv } = require('node:kvstore');
```

This module is only available under the `node:` scheme. The current
implementation is backed by the `node:sqlite` module, and every API it
exposes is synchronous (matching `node:sqlite`'s own `DatabaseSync`).

The following example shows the basic usage of the `node:kvstore` module to
open an in-memory key-value database, write data to the database, and then
read the data back.

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

// set few values
kv.set('testing-key', { text: 'Hello' });
kv.set('other-key', { text: 'World' });

// fetch multiple keys
const entries = kv.getMany(['testing-key', 'other-key']);
// Log the entries for the given keys
console.log(entries);
// Prints: [{key: ['testing-key'], value: {text: "Hello"}}, {key: ['other-key'], value: {text: "World"}}]

// get the list of keys within the store as an iterator
for (const key of kv.keys({ prefix: [] })) {
  // Log the list of keys in the store
  console.log(key);
  // Prints: { key: ['testing-key'] }
  // Prints: { key: ['other-key'] }
}
```

```cjs
'use strict';
const { openKv } = require('node:kvstore');
const kv = openKv();

// set few values
kv.set('testing-key', { text: 'Hello' });
kv.set('other-key', { text: 'World' });

// fetch multiple keys
const entries = kv.getMany(['testing-key', 'other-key']);
// Log the entries for the given keys
console.log(entries);
// Prints: [{key: ['testing-key'], value: {text: "Hello"}}, {key: ['other-key'], value: {text: "World"}}]

// get the list of keys within the store as an iterator
for (const key of kv.keys({ prefix: [] })) {
  // Log the list of keys in the store
  console.log(key);
  // Prints: { key: ['testing-key'] }
  // Prints: { key: ['other-key'] }
}
```

## The `KeyValue` type

A key can be either a `string` or an array of `string`, `boolean`, `number`,
or `bigint` values. The `string` notation is just a shorthand for `[string]`.

Composite keys sort in a fixed, well-defined order — both across segments of
the same type and across types: `string < number < bigint < boolean`. This
makes range and prefix queries ("give me everything between key A and key
B") behave the way you'd expect from any ordered key-value store.

Segment-level notes:

* **Strings** sort by UTF-8 byte order. Any string is valid except the
  literal empty string `''` — whitespace-only strings (e.g. `' '`) are fine.
* **Numbers** are IEEE-754 doubles and sort numerically (not
  lexicographically): `-300 < -5 < 0 < 5 < 300`. `NaN` is **not** a valid key
  segment (it can't be compared equal to itself) and throws.
  `Infinity`/`-Infinity` are valid and sort at the extreme ends of the number
  range. `-0` is normalized to `0` — they encode and compare identically.
* **Bigints** sort numerically with arbitrary precision, up to a magnitude of
  255 bytes (about 2040 bits — far beyond any realistic id or timestamp). A
  larger bigint throws `ERR_KVSTORE_BIGINT_TOO_LARGE` (see [Errors][] below).
* **Booleans** sort `false < true`.

A key's encoded form must not exceed 1024 bytes; oversized keys throw.

## Type conversion between JavaScript and stored values

Values passed to [`kv.set(key, value)`][] are serialized with Node's
`v8.serialize` (the same algorithm used by `worker_threads.postMessage`). The
following round-trip losslessly:

* primitives, including `undefined` and `BigInt`
* `Date`, `RegExp`, `Map`, `Set`, typed arrays, `ArrayBuffer`, `DataView`
* sparse arrays and circular references

What's **not** preserved:

* functions (cannot be cloned)
* class identity — instances round-trip as plain objects/Maps/Sets
* DOM-only types (irrelevant in Node)

The on-disk format is V8-internal: stable across Node versions, but not
portable to other runtimes (Bun, Deno, browsers). Pick a wire format like
CBOR if cross-runtime portability matters.

## Class: `KVStore`

<!-- YAML
added: v27.0.0
-->

This class represents a single connection to a KV store. All APIs exposed by
this class execute synchronously. Instances are created with
[`kvstore.openKv([options])`][], not with `new` — `KVStore` is exported only
so callers can perform `instanceof` checks and reference the type.

**Blocking I/O.** Every method that touches the database ([`kv.get(key)`][],
[`kv.set(key, value)`][], [`kv.delete(key)`][], [`kv.clear()`][],
[`kv.keys(selector)`][], [`kv.getMany(keys)`][]) blocks the event loop for the
duration of the underlying `node:sqlite` call — there is no async variant,
matching `node:sqlite`'s own `DatabaseSync`. For an in-memory store this is
sub-microsecond per call and rarely noticeable. For a file-backed store, each
write also waits on a disk `fsync` (mitigated, not eliminated, by the
always-on `WAL` mode described under [`kvstore.openKv([options])`][] below):
on a representative local SSD, `kv.set()` on a file-backed store measured
around 20 microseconds per call (~47,000 ops/sec) — small in isolation, but
on a single-threaded event loop every concurrent request pays that latency
serially. Avoid file-backed `KVStore` calls directly inside a hot request
path serving concurrent traffic; prefer batching writes, moving them off the
request path, or using a worker thread if write volume is high.

### `kv.clear()`

<!-- YAML
added: v27.0.0
-->

Deletes every key in the database.

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

kv.set('a', 1);
kv.set('b', 2);
kv.clear();
console.log(kv.get('a').value); // null
kv.close();
```

```cjs
const { openKv } = require('node:kvstore');
const kv = openKv();

kv.set('a', 1);
kv.set('b', 2);
kv.clear();
console.log(kv.get('a').value); // null
kv.close();
```

### `kv.close()`

<!-- YAML
added: v27.0.0
-->

Closes the connection to the store. If the connection is already closed,
this method does nothing.

### `kv.delete(key)`

<!-- YAML
added: v27.0.0
-->

* `key` {KeyValue} The key to delete.
* Returns: {boolean} `true` if an entry was deleted, `false` otherwise.

Removes the entry associated with `key`, if one exists.

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

kv.set(['user', 1], { name: 'alice' });
console.log(kv.delete(['user', 1])); // true
console.log(kv.delete(['user', 1])); // false (already absent)
kv.close();
```

```cjs
const { openKv } = require('node:kvstore');
const kv = openKv();

kv.set(['user', 1], { name: 'alice' });
console.log(kv.delete(['user', 1])); // true
console.log(kv.delete(['user', 1])); // false (already absent)
kv.close();
```

### `kv.get(key)`

<!-- YAML
added: v27.0.0
-->

* `key` {KeyValue}
* Returns: {Object}
  * `key` {KeyValue}
  * `value` {any} `null` if no value exists for the given key.

Retrieves the value associated with `key`.

**Note:** a stored value that genuinely *is* `null` is indistinguishable
from an absent key in 1.0 — both return `value: null`. Disambiguating the
two (e.g. via a returned `versionstamp`) is tracked for a 1.x release;
there's no workaround today beyond not storing `null` as a value where the
distinction matters to your application.

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

kv.set('hello', { text: 'World' });

console.log(kv.get('hello'));
// Prints: {key: ['hello'], value: {text: "World"}}
```

```cjs
const { openKv } = require('node:kvstore');
const kv = openKv();

kv.set('hello', { text: 'World' });

console.log(kv.get('hello'));
// Prints: {key: ['hello'], value: {text: "World"}}
```

### `kv.getMany(keys)`

<!-- YAML
added: v27.0.0
-->

* `keys` {KeyValue\[]} An array of keys to retrieve from the store.
* Returns: {Object\[]} An array the same length as `keys`, with entries in
  the same order as `keys`. Each entry has the same shape as
  [`kv.get(key)`][]'s return value.

Retrieves the values associated with each of `keys`. If no value exists for
a given key, its entry has a `null` value (same caveat as `kv.get()` above).

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

kv.set(['user', 1], 'alice');
kv.set(['user', 2], 'bob');
kv.set(['user', 3], 'carol');

// Output order matches input order, not storage order.
console.log(kv.getMany([['user', 3], ['user', 1]]).map((e) => e.value));
// Prints: ['carol', 'alice']

// Missing keys yield null in their position.
console.log(kv.getMany([['user', 1], ['user', 99]]).map((e) => e.value));
// Prints: ['alice', null]

kv.close();
```

```cjs
const { openKv } = require('node:kvstore');
const kv = openKv();

kv.set(['user', 1], 'alice');
kv.set(['user', 2], 'bob');
kv.set(['user', 3], 'carol');

// Output order matches input order, not storage order.
console.log(kv.getMany([['user', 3], ['user', 1]]).map((e) => e.value));
// Prints: ['carol', 'alice']

// Missing keys yield null in their position.
console.log(kv.getMany([['user', 1], ['user', 99]]).map((e) => e.value));
// Prints: ['alice', null]

kv.close();
```

### `kv.keys(selector)`

<!-- YAML
added: v27.0.0
-->

* `selector` {Object} Either a prefix-based selector or a range-based
  selector:
  * `prefix` {KeyValue} Matches keys nested under `prefix`, never the prefix
    key itself. An empty prefix (`[]`) matches every key in the store — this
    is the recipe for a full scan; there is no separate "list everything"
    method, precisely so that scanning the whole store is always an
    explicit, visible choice at the call site.
  * `start` {KeyValue} Inclusive lower bound. Can be combined with `prefix`
    as an offset (but not together with `end` both falling outside the
    prefix's key space).
  * `end` {KeyValue} Inclusive upper bound. Same combination rules as
    `start`.
* Returns: {Iterable} An iterator of `{ key: KeyValue }` objects.

Retrieves the keys matching `selector`.

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

kv.set('testing-key', { text: 'Hello' });
kv.set('other-key', { text: 'World' });

// get the list of keys within the store as an iterator
for (const key of kv.keys({ prefix: [] })) {
  // Log the list of keys in the store
  console.log(key);
  // Prints: { key: ['testing-key'] }
  // Prints: { key: ['other-key'] }
}
```

```cjs
const { openKv } = require('node:kvstore');
const kv = openKv();

kv.set('testing-key', { text: 'Hello' });
kv.set('other-key', { text: 'World' });

// get the list of keys within the store as an iterator
for (const key of kv.keys({ prefix: [] })) {
  // Log the list of keys in the store
  console.log(key);
  // Prints: { key: ['testing-key'] }
  // Prints: { key: ['other-key'] }
}
```

Range queries use `{ start, end }` (both inclusive) without a `prefix`:

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

for (const c of ['a', 'b', 'c', 'd', 'e']) kv.set([c], c);

const inRange = [];
for (const { key } of kv.keys({ start: ['b'], end: ['d'] })) inRange.push(key[0]);
console.log(inRange); // ['b', 'c', 'd']

// String and numeric segments with the same printed value are distinct keys
// because of type tags in the encoded form.
kv.set([5], 'numeric');
kv.set(['5'], 'string');
const fives = [];
for (const { key } of kv.keys({ start: [5], end: [5] })) fives.push(key);
console.log(fives); // [[5]] — only the numeric key
kv.close();
```

```cjs
const { openKv } = require('node:kvstore');
const kv = openKv();

for (const c of ['a', 'b', 'c', 'd', 'e']) kv.set([c], c);

const inRange = [];
for (const { key } of kv.keys({ start: ['b'], end: ['d'] })) inRange.push(key[0]);
console.log(inRange); // ['b', 'c', 'd']

// String and numeric segments with the same printed value are distinct keys
// because of type tags in the encoded form.
kv.set([5], 'numeric');
kv.set(['5'], 'string');
const fives = [];
for (const { key } of kv.keys({ start: [5], end: [5] })) fives.push(key);
console.log(fives); // [[5]] — only the numeric key
kv.close();
```

### `kv.publish(topic, payload)`

<!-- YAML
added: v27.0.0
-->

* `topic` {string} A non-empty topic name.
* `payload` {any} Any value, delivered to topic subscribers as-is.

Publishes `payload` to `topic`. Topics live in their own namespace,
independent of the key store — see [Topic plane][] below.

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

// Topic watchers receive the published payload directly (no type envelope).
const sub = kv.watch({ topic: 'events.signup' });
sub.on('data', (payload) => console.log('signup:', payload.userId));

kv.publish('events.signup', { userId: 42 }); // signup: 42
kv.publish('events.signup', { userId: 43 }); // signup: 43
kv.publish('events.logout', { userId: 42 }); // different topic — sub ignores this

sub.destroy();
kv.close();
```

```cjs
const { openKv } = require('node:kvstore');
const kv = openKv();

// Topic watchers receive the published payload directly (no type envelope).
const sub = kv.watch({ topic: 'events.signup' });
sub.on('data', (payload) => console.log('signup:', payload.userId));

kv.publish('events.signup', { userId: 42 }); // signup: 42
kv.publish('events.signup', { userId: 43 }); // signup: 43
kv.publish('events.logout', { userId: 42 }); // different topic — sub ignores this

sub.destroy();
kv.close();
```

### `kv.set(key, value)`

<!-- YAML
added: v27.0.0
-->

* `key` {KeyValue} The key to set.
* `value` {any} Any structured-cloneable value to store. See
  [Type conversion between JavaScript and stored values][].
* Returns: `undefined`

Sets the value associated with `key`, overwriting any existing value at that
key. Throws on error; otherwise returns nothing — there's no `{ ok }`
envelope, since under SQLite UPSERT semantics a no-op write of an identical
value would also report zero changed rows, and `{ ok: false }` would
misleadingly read as "the write failed."

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

kv.set('testing-key', { text: 'Hello' });
// ...
kv.set('testing-key', { text: 'World' });

console.log(kv.get('testing-key'));
// Prints { key: ['testing-key'], value: {text: 'World'}}
```

```cjs
const { openKv } = require('node:kvstore');
const kv = openKv();

kv.set('testing-key', { text: 'Hello' });
// ...
kv.set('testing-key', { text: 'World' });

console.log(kv.get('testing-key'));
// Prints { key: ['testing-key'], value: {text: 'World'}}
```

### `kv.watch(selector[, options])`

<!-- YAML
added: v27.0.0
-->

* `selector` {Object} Exactly one of:
  * `key` {KeyValue} Fires on mutations to that exact key (and on `clear`).
    Also accepts `events` (see below).
  * `keys` {KeyValue\[]} Fires on mutations to any of the listed exact keys
    (and on `clear`). Also accepts `events`.
  * `prefix` {KeyValue} Fires on mutations to any key under the prefix (and
    on `clear`). An empty prefix (`[]`) watches every key-plane mutation in
    the store. Also accepts `events`.
  * `topic` {string} Fires on [`kv.publish(topic, payload)`][] calls.
  * `events` {string\[]} For the `key`/`keys`/`prefix` forms, restricts
    delivery to a subset of `'set'`, `'delete'`, `'clear'`.
* `options` {Object}
  * `highWaterMark` {number} Per-watcher buffer size. Must be `>= 1` (throws
    otherwise). If the buffer is full, intervening events are dropped and a
    single `{type:'lag', dropped}` event is delivered once space frees up.
    **Default:** `1024`.
* Returns: {stream.Readable} An object-mode `Readable`, also usable as an
  `AsyncIterable`, that emits one event per push. Call `stream.destroy()` to
  unsubscribe.

Watches mutations on the store, or messages on a custom topic.

**Scope.** Like the topic plane (see [Topic plane][] below), `key`/`keys`/
`prefix` watchers only observe mutations made through the *same* `KVStore`
instance, in the same process. A file-backed store mutated by another
process, or by a second `openKv({ path })` call in the same process pointed
at the same file, produces no watch events on this instance — `node:sqlite`
gives this module no cross-connection change feed to build on. If you need
writes from another process or instance to be observable, poll
[`kv.keys(selector)`][] instead.

Key / keys / prefix watchers emit one of:

```js
{ type: 'set',    key: KeyValue, value: any }
{ type: 'delete', key: KeyValue }
{ type: 'clear' }
{ type: 'lag',    dropped: number }   // see Backpressure
```

Topic watchers emit the published payload directly (no envelope).

```mjs
const sub = kv.watch({ prefix: ['user'] });
for await (const event of sub) {
  // { type: 'set', key: ['user', 42], value: {...} } | ...
}
```

The optional `events` array narrows delivery to a subset of mutation types.
This is useful when you only care about new arrivals (writes) and want to
ignore deletes and `clear`:

```mjs
import { openKv } from 'node:kvstore';
const kv = openKv();

// Only fires on 'set' — deletes and clear are silently discarded.
const arrivals = kv.watch({ prefix: ['inbox'], events: ['set'] });
arrivals.on('data', ({ key, value }) =>
  console.log(`new message at ${JSON.stringify(key)} from ${value.from}`));

kv.set(['inbox', 1, 'msg-1'], { from: 'alice' }); // fires
kv.delete(['inbox', 1, 'msg-1']);                  // filtered out
arrivals.destroy();
kv.close();
```

```cjs
const { openKv } = require('node:kvstore');
const kv = openKv();

// Only fires on 'set' — deletes and clear are silently discarded.
const arrivals = kv.watch({ prefix: ['inbox'], events: ['set'] });
arrivals.on('data', ({ key, value }) =>
  console.log(`new message at ${JSON.stringify(key)} from ${value.from}`));

kv.set(['inbox', 1, 'msg-1'], { from: 'alice' }); // fires
kv.delete(['inbox', 1, 'msg-1']);                  // filtered out
arrivals.destroy();
kv.close();
```

> **Migration from 0.x.** The legacy positional form `kv.watch(keys[])` has
> been replaced. Use `kv.watch({ keys: [...] })` instead, and note that the
> stream now emits one event per change (not a positional `entries[]` array
> with `null` placeholders).

#### Topic plane

`kv.publish()`/`kv.watch({ topic })` is a separate plane from key-value
mutations, with its own guarantees:

* **Scope.** Topics live only in the same Node process as the `KVStore`
  instance — they do not cross process boundaries, do not persist to disk,
  and do not cross machines. Restarting the process, or opening a second
  `KVStore` pointed at the same file, gets no history.
* **Isolation from the key plane.** `publish()` never touches the underlying
  table; key-plane `set`/`delete`/`clear` never appear on topic streams;
  topic names share no namespace with keys (a topic named `'user'` and a key
  `['user']` are unrelated).
* **Ordering.** Within a single topic, payloads delivered to one watcher
  arrive FIFO (publish order). There is no ordering guarantee *across*
  different topics.
* **Delivery.** Best-effort, in-process, at-most-once. No retries, no
  persistence — a payload published while no watcher is attached to that
  topic is simply gone.
* **Lifecycle.** Topic streams end when [`kv.close()`][] is called, exactly
  like key-plane watchers.

#### Backpressure

Watchers must consume events in a timely manner. Each watcher has its own
independent buffer — a slow watcher never affects any other watcher or the
publisher.

* **Drop policy.** When a watcher's buffer reaches `highWaterMark`, the
  *new* incoming event is dropped (the buffer keeps what it already has,
  rather than evicting older events).
* **`lag` event.** Exactly one `{type:'lag', dropped}` event is emitted per
  drop episode, once the buffer has drained back below `highWaterMark`, and
  always *before* the next non-`lag` event is delivered. `dropped` counts
  every event lost since the previous `lag` event (or since the stream
  started) and is always `> 0`.
* **`close()` during a drop window.** [`kv.close()`][] ends every
  outstanding stream immediately. Any pending `lag` event for an unflushed
  drop window is **not** emitted — the stream simply ends.

## `kvstore.openKv([options])`

<!-- YAML
added: v27.0.0
-->

* `options` {Object} Configuration options for the store connection.
  * `path` {string} On-disk database file. When omitted, the store is
    ephemeral (in-memory) — `openKv()` with no arguments has no disk side
    effect.
* Returns: {KVStore}

Creates a new [`KVStore`][] instance and establishes a connection to it. This
is the documented entry point for obtaining a `KVStore`; all APIs it exposes
execute synchronously (unless stated otherwise).

```mjs
import { openKv } from 'node:kvstore';

const ephemeral = openKv();
const persistent = openKv({ path: './my-store.sqlite' });
```

```cjs
'use strict';
const { openKv } = require('node:kvstore');

const ephemeral = openKv();
const persistent = openKv({ path: './my-store.sqlite' });
```

**Storage.** A file-backed store runs with `PRAGMA journal_mode=WAL` and
`PRAGMA synchronous=NORMAL` always on (there is no option to disable this).
This means two sidecar files, `<path>-wal` and `<path>-shm`, exist alongside
the database file while it's open. They're removed automatically on a clean
[`kv.close()`][]. If you copy or back up the database file while a store has
it open, copy the sidecars too (or checkpoint first) — copying just the
`.sqlite` file mid-write is not safe.

**Security: only open files you trust.** `path` must point at a database
this module (or a trusted process) created. A stored value is read back with
[`v8.deserialize()`][] (see [Type conversion between JavaScript and stored
values][]), which — like `JSON.parse()`, but for the V8-internal wire format
— executes as soon as any value in the file is read, including via
[`kv.keys(selector)`][] range/prefix scans. Opening a `path` containing
attacker-controlled bytes means running `v8.deserialize()` over data you
don't control; treat an untrusted `.sqlite` file the same way you'd treat any
other untrusted serialized-object input, and don't accept one from a
different trust boundary (e.g. a file uploaded by a user) without validating
it out-of-band first.

## Errors

Every error thrown by this module is a plain `Error`, `TypeError`, or
`RangeError` instance carrying a stable `.code` string, so callers can branch
on failure kind without parsing `.message`:

| `.code`                         | Base         | Thrown when                                                                                               |
| ------------------------------- | ------------ | --------------------------------------------------------------------------------------------------------- |
| `ERR_INVALID_ARG_TYPE`          | `TypeError`  | A key segment, argument, or selector field has the wrong JavaScript type.                                 |
| `ERR_INVALID_ARG_VALUE`         | `TypeError`  | A key segment or argument has the right type but an invalid value (e.g. `NaN`, empty string, or an oversized encoded key). |
| `ERR_OUT_OF_RANGE`              | `RangeError` | A numeric argument is outside its valid range (e.g. `highWaterMark < 1`).                                |
| `ERR_KVSTORE_CLOSED`            | `Error`      | Any method called after [`kv.close()`][].                                                                 |
| `ERR_KVSTORE_BIGINT_TOO_LARGE`  | `RangeError` | A bigint key segment's magnitude exceeds 255 bytes.                                                       |
| `ERR_INVALID_STATE`             | `Error`      | Stored data is corrupt (cannot be decoded as a valid key or value).                                       |

```mjs
import { openKv } from 'node:kvstore';

const kv = openKv();
kv.close();
try {
  kv.get('x');
} catch (err) {
  if (err.code === 'ERR_KVSTORE_CLOSED') {
    console.log(err instanceof Error); // true
  }
}
```

```cjs
'use strict';
const { openKv } = require('node:kvstore');

const kv = openKv();
kv.close();
try {
  kv.get('x');
} catch (err) {
  if (err.code === 'ERR_KVSTORE_CLOSED') {
    console.log(err instanceof Error); // true
  }
}
```

## Examples

The following examples use CJS (`require`). ESM (`import { openKv } from 'node:kvstore'`)
works identically.

### Namespace isolation via composite key prefixes

Multiple logical "tables" can coexist in a single store by prefixing every
key with a namespace segment. Because `kv.keys({ prefix })`, `kv.watch({
prefix })`, and `kv.delete()` all operate on encoded key ranges, the isolation
is enforced by the codec — no `namespace` option is needed.

```cjs
'use strict';
const { openKv } = require('node:kvstore');
const kv = openKv();

// Wrap a KVStore with a fixed namespace prefix.
function ns(namespace) {
  const pfx = (key) => [namespace, ...(Array.isArray(key) ? key : [key])];
  return {
    get:    (key) => kv.get(pfx(key)),
    set:    (key, value) => kv.set(pfx(key), value),
    delete: (key) => kv.delete(pfx(key)),
    keys:   (sel) => kv.keys({ ...sel, prefix: [namespace, ...(sel?.prefix ?? [])] }),
  };
}

const users = ns('users');
const cache = ns('cache');

users.set(42, { name: 'alice' });
cache.set(42, { etag: 'abc-123' });

// Same logical key, different namespace — no collision.
console.log(users.get(42).value); // { name: 'alice' }
console.log(cache.get(42).value); // { etag: 'abc-123' }

users.delete(42);
console.log(users.get(42).value); // null
console.log(cache.get(42).value); // { etag: 'abc-123' } (untouched)

kv.close();
```

### Cache with TTL

The store has no built-in TTL; implement one by storing `{ value, expiresAt }`
envelopes and validating expiry on read. A `cleanup()` sweep can be called
periodically or at startup to evict stale entries in bulk.

```cjs
'use strict';
const { openKv } = require('node:kvstore');
const kv = openKv();
const NS = 'cache';

function put(name, value, ttlMs) {
  kv.set([NS, name], { value, expiresAt: Date.now() + ttlMs });
}

function fetch(name) {
  const { value: env } = kv.get([NS, name]);
  if (!env) return null;
  if (env.expiresAt <= Date.now()) {
    kv.delete([NS, name]);
    return null;
  }
  return env.value;
}

function cleanup() {
  let evicted = 0;
  for (const { key } of kv.keys({ prefix: [NS] })) {
    const { value: env } = kv.get(key);
    if (env && env.expiresAt <= Date.now()) { kv.delete(key); evicted++; }
  }
  return evicted;
}

put('config', { debug: true }, 60_000); // 60 s TTL
console.log(fetch('config'));            // { debug: true }
console.log(fetch('missing'));           // null

kv.close();
```

### Session store

A session store combining key-plane persistence with topic-plane event
broadcasting. Every mutation is audited by a prefix watcher; login/logout
events are broadcast on separate topics for other consumers (analytics,
notifications, etc.).

```cjs
'use strict';
const { openKv } = require('node:kvstore');
const kv = openKv();

// Audit trail: record every session mutation.
const auditLog = [];
const audit = kv.watch({ prefix: ['session'] });
audit.on('data', (e) => auditLog.push({ op: e.type, key: e.key }));

// App-level event feed: subscribe to login events.
const loginFeed = kv.watch({ topic: 'login' });
loginFeed.on('data', (user) => console.log(`logged in: ${user}`));

function login(id, user) {
  kv.set(['session', id], { user, lastSeen: Date.now() });
  kv.publish('login', user);
}

function touch(id) {
  const { value } = kv.get(['session', id]);
  if (!value) return false;
  kv.set(['session', id], { ...value, lastSeen: Date.now() });
  return true;
}

function logout(id) {
  const { value } = kv.get(['session', id]);
  if (!value) return false;
  kv.delete(['session', id]);
  kv.publish('logout', value.user);
  return true;
}

login('s-1', 'alice');  // logged in: alice
login('s-2', 'bob');    // logged in: bob
touch('s-1');
logout('s-2');

// auditLog (after the event loop drains) contains:
// [{ op: 'set',    key: ['session', 's-1'] },
//  { op: 'set',    key: ['session', 's-2'] },
//  { op: 'set',    key: ['session', 's-1'] },  ← touch refresh
//  { op: 'delete', key: ['session', 's-2'] }]

audit.destroy();
loginFeed.destroy();
kv.close();
```

### Work queue

A multi-producer / single-consumer queue using a topic as a lightweight
wake-up signal and the key store as the durable backlog. The topic fires
the worker immediately when new work arrives; the worker drains the queue
by scanning the `['queue']` prefix until it is empty.

```cjs
'use strict';
const { openKv } = require('node:kvstore');
const kv = openKv();

let nextId = 0;

function enqueue(task) {
  kv.set(['queue', ++nextId], task);
  kv.publish('queue.new', nextId);
}

function claimNext() {
  for (const { key } of kv.keys({ prefix: ['queue'] })) {
    const { value } = kv.get(key);
    if (kv.delete(key)) return { id: key[1], task: value };
  }
  return null;
}

const wakeups = kv.watch({ topic: 'queue.new' });
wakeups.on('data', () => {
  let item;
  while ((item = claimNext())) {
    console.log(`processing ${item.id}: ${item.task.kind}`);
  }
});

enqueue({ kind: 'email',   to: 'alice' });
enqueue({ kind: 'webhook', url: 'https://example.com' });
enqueue({ kind: 'email',   to: 'bob' });

// (after event loop drains)
// processing 1: email
// processing 2: webhook
// processing 3: email

wakeups.destroy();
kv.close();
```

### Reactive audit log

A watcher on `['data']` writes an append-only entry under `['audit']` for
every mutation. Because the auditor writes to a *different* prefix it cannot
trigger itself recursively.

```cjs
'use strict';
const { openKv } = require('node:kvstore');
const kv = openKv();

let seq = 0;
const watcher = kv.watch({ prefix: ['data'] });
watcher.on('data', (e) => {
  if (e.type === 'lag' || e.type === 'clear') return;
  kv.set(['audit', ++seq], { at: Date.now(), op: e.type, key: e.key });
});

kv.set(['data', 'user', 1], { name: 'alice' });
kv.set(['data', 'user', 2], { name: 'bob' });
kv.set(['data', 'user', 1], { name: 'alice', role: 'admin' });
kv.delete(['data', 'user', 2]);

// (after event loop drains)
// kv.keys({ prefix: ['audit'] }) yields 4 entries, one per mutation above.

watcher.destroy();
kv.close();
```

[Errors]: #errors
[Topic plane]: #topic-plane
[Type conversion between JavaScript and stored values]: #type-conversion-between-javascript-and-stored-values
[`KVStore`]: #class-kvstore
[`kv.clear()`]: #kvclear
[`kv.close()`]: #kvclose
[`kv.delete(key)`]: #kvdeletekey
[`kv.get(key)`]: #kvgetkey
[`kv.getMany(keys)`]: #kvgetmanykeys
[`kv.keys(selector)`]: #kvkeysselector
[`kv.publish(topic, payload)`]: #kvpublishtopic-payload
[`kv.set(key, value)`]: #kvsetkey-value
[`v8.deserialize()`]: v8.md#v8deserializebuffer
[`kvstore.openKv([options])`]: #kvstoreopenkvoptions
