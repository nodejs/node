# Stream (new)

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

<!-- source_link=lib/stream/new.js -->

The `node:stream/new` module provides a new streaming API built on iterables
rather than the event-driven `Readable`/`Writable`/`Transform` class hierarchy.

Streams are represented as `AsyncIterable<Uint8Array[]>` (async) or
`Iterable<Uint8Array[]>` (sync). There are no base classes to extend -- any
object implementing the iterable protocol can participate. Transforms are plain
functions or objects with a `transform` method.

Data flows in **batches** (`Uint8Array[]` per iteration) to amortize the cost
of async operations.

```mjs
import { from, pull, text, compressGzip, decompressGzip } from 'node:stream/new';

// Compress and decompress a string
const compressed = pull(from('Hello, world!'), compressGzip());
const result = await text(pull(compressed, decompressGzip()));
console.log(result); // 'Hello, world!'
```

```cjs
const { from, pull, text, compressGzip, decompressGzip } = require('node:stream/new');

async function run() {
  // Compress and decompress a string
  const compressed = pull(from('Hello, world!'), compressGzip());
  const result = await text(pull(compressed, decompressGzip()));
  console.log(result); // 'Hello, world!'
}

run().catch(console.error);
```

```mjs
import { open } from 'node:fs/promises';
import { text, compressGzip, decompressGzip, pipeTo } from 'node:stream/new';

// Read a file, compress, write to another file
const src = await open('input.txt', 'r');
const dst = await open('output.gz', 'w');
await pipeTo(src.pull(), compressGzip(), dst.writer({ autoClose: true }));
await src.close();

// Read it back
const gz = await open('output.gz', 'r');
console.log(await text(gz.pull(decompressGzip(), { autoClose: true })));
```

```cjs
const { open } = require('node:fs/promises');
const { text, compressGzip, decompressGzip, pipeTo } = require('node:stream/new');

async function run() {
  // Read a file, compress, write to another file
  const src = await open('input.txt', 'r');
  const dst = await open('output.gz', 'w');
  await pipeTo(src.pull(), compressGzip(), dst.writer({ autoClose: true }));
  await src.close();

  // Read it back
  const gz = await open('output.gz', 'r');
  console.log(await text(gz.pull(decompressGzip(), { autoClose: true })));
}

run().catch(console.error);
```

## Concepts

### Byte streams

All data in the new streams API is represented as `Uint8Array` bytes. Strings
are automatically UTF-8 encoded when passed to `from()`, `push()`, or
`pipeTo()`. This removes ambiguity around encodings and enables zero-copy
transfers between streams and native code.

### Batching

Each iteration yields a **batch** -- an array of `Uint8Array` chunks
(`Uint8Array[]`). Batching amortizes the cost of `await` and Promise creation
across multiple chunks. A consumer that processes one chunk at a time can
simply iterate the inner array:

```mjs
for await (const batch of source) {
  for (const chunk of batch) {
    handle(chunk);
  }
}
```

```cjs
async function run() {
  for await (const batch of source) {
    for (const chunk of batch) {
      handle(chunk);
    }
  }
}
```

### Transforms

Transforms come in two forms:

* **Stateless** -- a function `(chunks, options) => result` called once per
  batch. Receives `Uint8Array[]` (or `null` as the flush signal) and an
  `options` object. Returns `Uint8Array[]`, `null`, or an iterable of chunks.

* **Stateful** -- an object `{ transform(source, options) }` where `transform`
  is a generator (sync or async) that receives the entire upstream iterable
  and an `options` object, and yields output. This form is used for
  compression, encryption, and any transform that needs to buffer across
  batches.

Both forms receive an `options` parameter with the following property:

* `options.signal` {AbortSignal} An AbortSignal that fires when the pipeline
  is cancelled, encounters an error, or the consumer stops reading. Transforms
  can check `signal.aborted` or listen for the `'abort'` event to perform
  early cleanup.

The flush signal (`null`) is sent after the source ends, giving transforms
a chance to emit trailing data (e.g., compression footers).

```js
// Stateless: uppercase transform
const upper = (chunks) => {
  if (chunks === null) return null; // flush
  return chunks.map((c) => new TextEncoder().encode(
    new TextDecoder().decode(c).toUpperCase(),
  ));
};

// Stateful: line splitter
const lines = {
  transform: async function*(source) {
    let partial = '';
    for await (const chunks of source) {
      if (chunks === null) {
        if (partial) yield [new TextEncoder().encode(partial)];
        continue;
      }
      for (const chunk of chunks) {
        const str = partial + new TextDecoder().decode(chunk);
        const parts = str.split('\n');
        partial = parts.pop();
        for (const line of parts) {
          yield [new TextEncoder().encode(`${line}\n`)];
        }
      }
    }
  },
};
```

### Pull vs. push

The API supports two models:

* **Pull** -- data flows on demand. `pull()` and `pullSync()` create lazy
  pipelines that only read from the source when the consumer iterates.

* **Push** -- data is written explicitly. `push()` creates a writer/readable
  pair with backpressure. The writer pushes data in; the readable is consumed
  as an async iterable.

### Backpressure

Pull streams have natural backpressure -- the consumer drives the pace, so
the source is never read faster than the consumer can process. Push streams
need explicit backpressure because the producer and consumer run
independently. The `highWaterMark` and `backpressure` options on `push()`,
`broadcast()`, and `share()` control how this works.

#### The two-buffer model

Push streams use a two-part buffering system. Think of it like a bucket
(slots) being filled through a hose (pending writes), with a float valve
that closes when the bucket is full:

```text
                          highWaterMark (e.g., 3)
                                 |
    Producer                     v
       |                    +---------+
       v                    |         |
  [ write() ] ----+    +--->| slots   |---> Consumer pulls
  [ write() ]     |    |    | (bucket)|     for await (...)
  [ write() ]     v    |    +---------+
              +--------+         ^
              | pending|         |
              | writes |    float valve
              | (hose) |    (backpressure)
              +--------+
                   ^
                   |
          'strict' mode limits this too!
```

* **Slots (the bucket)** -- data ready for the consumer, capped at
  `highWaterMark`. When the consumer pulls, it drains all slots at once
  into a single batch.

* **Pending writes (the hose)** -- writes waiting for slot space. After
  the consumer drains, pending writes are promoted into the now-empty
  slots and their promises resolve.

How each policy uses these buffers:

| Policy          | Slots limit     | Pending writes limit |
| --------------- | --------------- | -------------------- |
| `'strict'`      | `highWaterMark` | `highWaterMark`      |
| `'block'`       | `highWaterMark` | Unbounded            |
| `'drop-oldest'` | `highWaterMark` | N/A (never waits)    |
| `'drop-newest'` | `highWaterMark` | N/A (never waits)    |

#### Strict (default)

Strict mode catches "fire-and-forget" patterns where the producer calls
`write()` without awaiting, which would cause unbounded memory growth.
It limits both the slots buffer and the pending writes queue to
`highWaterMark`.

If you properly await each write, you can only ever have one pending
write at a time (yours), so you never hit the pending writes limit.
Unawaited writes accumulate in the pending queue and throw once it
overflows:

```mjs
import { push, text } from 'node:stream/new';

const { writer, readable } = push({ highWaterMark: 16 });

// Consumer must run concurrently -- without it, the first write
// that fills the buffer blocks the producer forever.
const consuming = text(readable);

// GOOD: awaited writes. The producer waits for the consumer to
// make room when the buffer is full.
for (const item of dataset) {
  await writer.write(item);
}
await writer.end();
console.log(await consuming);
```

```cjs
const { push, text } = require('node:stream/new');

async function run() {
  const { writer, readable } = push({ highWaterMark: 16 });

  // Consumer must run concurrently -- without it, the first write
  // that fills the buffer blocks the producer forever.
  const consuming = text(readable);

  // GOOD: awaited writes. The producer waits for the consumer to
  // make room when the buffer is full.
  for (const item of dataset) {
    await writer.write(item);
  }
  await writer.end();
  console.log(await consuming);
}

run().catch(console.error);
```

Forgetting to `await` will eventually throw:

```js
// BAD: fire-and-forget. Strict mode throws once both buffers fill.
for (const item of dataset) {
  writer.write(item); // Not awaited -- queues without bound
}
// --> throws "Backpressure violation: too many pending writes"
```

This is the default policy because it catches the exact class of bug
that push streams exist to prevent.

#### Block

Block mode caps slots at `highWaterMark` but places no limit on the
pending writes queue. Awaited writes block until the consumer makes room,
just like strict mode. The difference is that unawaited writes silently
queue forever instead of throwing -- a potential memory leak if the
producer forgets to `await`.

This is the mode that existing Node.js classic streams and Web Streams
default to. Use it when you control the producer and know it awaits
properly, or when migrating code from those APIs.

```mjs
import { push, text } from 'node:stream/new';

const { writer, readable } = push({
  highWaterMark: 16,
  backpressure: 'block',
});

const consuming = text(readable);

// Safe -- awaited writes block until the consumer reads.
for (const item of dataset) {
  await writer.write(item);
}
await writer.end();
console.log(await consuming);
```

```cjs
const { push, text } = require('node:stream/new');

async function run() {
  const { writer, readable } = push({
    highWaterMark: 16,
    backpressure: 'block',
  });

  const consuming = text(readable);

  // Safe -- awaited writes block until the consumer reads.
  for (const item of dataset) {
    await writer.write(item);
  }
  await writer.end();
  console.log(await consuming);
}

run().catch(console.error);
```

#### Drop-oldest

Writes never wait. When the slots buffer is full, the oldest buffered
chunk is evicted to make room for the incoming write. The consumer
always sees the most recent data. Useful for live feeds, telemetry, or
any scenario where stale data is less valuable than current data.

```mjs
import { push } from 'node:stream/new';

// Keep only the 5 most recent readings
const { writer, readable } = push({
  highWaterMark: 5,
  backpressure: 'drop-oldest',
});
```

```cjs
const { push } = require('node:stream/new');

// Keep only the 5 most recent readings
const { writer, readable } = push({
  highWaterMark: 5,
  backpressure: 'drop-oldest',
});
```

#### Drop-newest

Writes never wait. When the slots buffer is full, the incoming write is
silently discarded. The consumer processes what is already buffered
without being overwhelmed by new data. Useful for rate-limiting or
shedding load under pressure.

```mjs
import { push } from 'node:stream/new';

// Accept up to 10 buffered items; discard anything beyond that
const { writer, readable } = push({
  highWaterMark: 10,
  backpressure: 'drop-newest',
});
```

```cjs
const { push } = require('node:stream/new');

// Accept up to 10 buffered items; discard anything beyond that
const { writer, readable } = push({
  highWaterMark: 10,
  backpressure: 'drop-newest',
});
```

### Writers

A writer is any object with a `write(chunk)` method. Writers optionally
support `writev(chunks)` for batch writes (mapped to scatter/gather I/O where
available), `end()` to signal completion, and `fail(reason)` to signal
failure.

## `require('node:stream/new')`

All functions are available both as named exports and as properties of the
`Stream` namespace object:

```mjs
// Named exports
import { from, pull, bytes, Stream } from 'node:stream/new';

// Namespace access
Stream.from('hello');
```

```cjs
// Named exports
const { from, pull, bytes, Stream } = require('node:stream/new');

// Namespace access
Stream.from('hello');
```

## Sources

### `from(input)`

<!-- YAML
added: REPLACEME
-->

* `input` {string|ArrayBuffer|ArrayBufferView|Iterable|AsyncIterable}
* Returns: {AsyncIterable\<Uint8Array\[]>}

Create an async byte stream from the given input. Strings are UTF-8 encoded.
`ArrayBuffer` and `ArrayBufferView` values are wrapped as `Uint8Array`. Arrays
and iterables are recursively flattened and normalized.

Objects implementing `Symbol.for('Stream.toAsyncStreamable')` or
`Symbol.for('Stream.toStreamable')` are converted via those protocols.

```mjs
import { Buffer } from 'node:buffer';
import { from, text } from 'node:stream/new';

console.log(await text(from('hello')));       // 'hello'
console.log(await text(from(Buffer.from('hello')))); // 'hello'
```

```cjs
const { from, text } = require('node:stream/new');

async function run() {
  console.log(await text(from('hello')));       // 'hello'
  console.log(await text(from(Buffer.from('hello')))); // 'hello'
}

run().catch(console.error);
```

### `fromSync(input)`

<!-- YAML
added: REPLACEME
-->

* `input` {string|ArrayBuffer|ArrayBufferView|Iterable}
* Returns: {Iterable\<Uint8Array\[]>}

Synchronous version of [`from()`][]. Returns a sync iterable. Cannot accept
async iterables or promises.

```mjs
import { fromSync, textSync } from 'node:stream/new';

console.log(textSync(fromSync('hello'))); // 'hello'
```

```cjs
const { fromSync, textSync } = require('node:stream/new');

console.log(textSync(fromSync('hello'))); // 'hello'
```

## Pipelines

### `pipeTo(source[, ...transforms], writer[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable|Iterable} The data source.
* `...transforms` {Function|Object} Zero or more transforms to apply.
* `writer` {Object} Destination with `write(chunk)` method.
* `options` {Object}
  * `signal` {AbortSignal} Abort the pipeline.
  * `preventClose` {boolean} If `true`, do not call `writer.end()` when
    the source ends. **Default:** `false`.
  * `preventFail` {boolean} If `true`, do not call `writer.fail()` on
    error. **Default:** `false`.
* Returns: {Promise\<number>} Total bytes written.

Pipe a source through transforms into a writer. If the writer has a
`writev(chunks)` method, entire batches are passed in a single call (enabling
scatter/gather I/O).

```mjs
import { from, pipeTo, compressGzip } from 'node:stream/new';
import { open } from 'node:fs/promises';

const fh = await open('output.gz', 'w');
const totalBytes = await pipeTo(
  from('Hello, world!'),
  compressGzip(),
  fh.writer({ autoClose: true }),
);
```

```cjs
const { from, pipeTo, compressGzip } = require('node:stream/new');
const { open } = require('node:fs/promises');

async function run() {
  const fh = await open('output.gz', 'w');
  const totalBytes = await pipeTo(
    from('Hello, world!'),
    compressGzip(),
    fh.writer({ autoClose: true }),
  );
}

run().catch(console.error);
```

### `pipeToSync(source[, ...transforms], writer[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {Iterable} The sync data source.
* `...transforms` {Function|Object} Zero or more sync transforms.
* `writer` {Object} Destination with `write(chunk)` method.
* `options` {Object}
  * `preventClose` {boolean} **Default:** `false`.
  * `preventFail` {boolean} **Default:** `false`.
* Returns: {number} Total bytes written.

Synchronous version of [`pipeTo()`][].

### `pull(source[, ...transforms][, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable|Iterable} The data source.
* `...transforms` {Function|Object} Zero or more transforms to apply.
* `options` {Object}
  * `signal` {AbortSignal} Abort the pipeline.
* Returns: {AsyncIterable\<Uint8Array\[]>}

Create a lazy async pipeline. Data is not read from `source` until the
returned iterable is consumed. Transforms are applied in order.

```mjs
import { from, pull, text } from 'node:stream/new';

const upper = (chunks) => {
  if (chunks === null) return null;
  return chunks.map((c) =>
    new TextEncoder().encode(new TextDecoder().decode(c).toUpperCase()),
  );
};

const result = pull(from('hello'), upper);
console.log(await text(result)); // 'HELLO'
```

```cjs
const { from, pull, text } = require('node:stream/new');

const upper = (chunks) => {
  if (chunks === null) return null;
  return chunks.map((c) =>
    new TextEncoder().encode(new TextDecoder().decode(c).toUpperCase()),
  );
};

async function run() {
  const result = pull(from('hello'), upper);
  console.log(await text(result)); // 'HELLO'
}

run().catch(console.error);
```

Using an `AbortSignal`:

```mjs
import { pull } from 'node:stream/new';

const ac = new AbortController();
const result = pull(source, transform, { signal: ac.signal });
ac.abort(); // Pipeline throws AbortError on next iteration
```

```cjs
const { pull } = require('node:stream/new');

const ac = new AbortController();
const result = pull(source, transform, { signal: ac.signal });
ac.abort(); // Pipeline throws AbortError on next iteration
```

### `pullSync(source[, ...transforms])`

<!-- YAML
added: REPLACEME
-->

* `source` {Iterable} The sync data source.
* `...transforms` {Function|Object} Zero or more sync transforms.
* Returns: {Iterable\<Uint8Array\[]>}

Synchronous version of [`pull()`][]. All transforms must be synchronous.

## Push streams

### `push([...transforms][, options])`

<!-- YAML
added: REPLACEME
-->

* `...transforms` {Function|Object} Optional transforms applied to the
  readable side.
* `options` {Object}
  * `highWaterMark` {number} Maximum number of buffered slots before
    backpressure is applied. Must be >= 1; values below 1 are clamped to 1.
    **Default:** `4`.
  * `backpressure` {string} Backpressure policy: `'strict'`, `'block'`,
    `'drop-oldest'`, or `'drop-newest'`. **Default:** `'strict'`.
  * `signal` {AbortSignal} Abort the stream.
* Returns: {Object}
  * `writer` {PushWriter} The writer side.
  * `readable` {AsyncIterable\<Uint8Array\[]>} The readable side.

Create a push stream with backpressure. The writer pushes data in; the
readable side is consumed as an async iterable.

```mjs
import { push, text } from 'node:stream/new';

const { writer, readable } = push();

// Producer and consumer must run concurrently. With strict backpressure
// (the default), awaited writes block until the consumer reads.
const producing = (async () => {
  await writer.write('hello');
  await writer.write(' world');
  await writer.end();
})();

console.log(await text(readable)); // 'hello world'
await producing;
```

```cjs
const { push, text } = require('node:stream/new');

async function run() {
  const { writer, readable } = push();

  // Producer and consumer must run concurrently. With strict backpressure
  // (the default), awaited writes block until the consumer reads.
  const producing = (async () => {
    await writer.write('hello');
    await writer.write(' world');
    await writer.end();
  })();

  console.log(await text(readable)); // 'hello world'
  await producing;
}

run().catch(console.error);
```

#### Writer

The writer returned by `push()` has the following methods:

##### `writer.fail(reason)`

* `reason` {Error}
* Returns: {Promise\<void>}

Fail the stream with an error.

##### `writer.failSync(reason)`

* `reason` {Error}

Synchronously fail the stream with an error. Does not return a promise.

##### `writer.desiredSize`

* {number|null}

The number of buffer slots available before the high water mark is reached.
Returns `null` if the writer is closed or the consumer has disconnected.

##### `writer.end([options])`

* `options` {Object}
  * `signal` {AbortSignal} Cancel just this operation. The signal cancels only
    the pending `end()` call; it does not fail the writer itself.
* Returns: {Promise\<number>} Total bytes written.

Signal that no more data will be written.

##### `writer.write(chunk[, options])`

* `chunk` {Uint8Array|string}
* `options` {Object}
  * `signal` {AbortSignal} Cancel just this write operation. The signal cancels
    only the pending `write()` call; it does not fail the writer itself.
* Returns: {Promise\<void>}

Write a chunk. The promise resolves when buffer space is available.

##### `writer.writeSync(chunk)`

* `chunk` {Uint8Array|string}
* Returns: {boolean} `true` if the write was accepted, `false` if the
  buffer is full.

Synchronous write. Does not block; returns `false` if backpressure is active.

##### `writer.writev(chunks[, options])`

* `chunks` {Uint8Array\[]|string\[]}
* `options` {Object}
  * `signal` {AbortSignal} Cancel just this write operation. The signal cancels
    only the pending `writev()` call; it does not fail the writer itself.
* Returns: {Promise\<void>}

Write multiple chunks as a single batch.

##### `writer.writevSync(chunks)`

* `chunks` {Uint8Array\[]|string\[]}
* Returns: {boolean}

Synchronous batch write.

## Consumers

### `array(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable\<Uint8Array\[]>|Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `signal` {AbortSignal}
  * `limit` {number}
* Returns: {Promise\<Uint8Array\[]>}

Collect all chunks as an array of `Uint8Array` values (without concatenating).

### `arrayBuffer(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable\<Uint8Array\[]>|Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `signal` {AbortSignal}
  * `limit` {number}
* Returns: {Promise\<ArrayBuffer>}

Collect all bytes into an `ArrayBuffer`.

### `arrayBufferSync(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `limit` {number}
* Returns: {ArrayBuffer}

Synchronous version of [`arrayBuffer()`][].

### `arraySync(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `limit` {number}
* Returns: {Uint8Array\[]}

Synchronous version of [`array()`][].

### `bytes(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable\<Uint8Array\[]>|Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `signal` {AbortSignal}
  * `limit` {number} Maximum bytes to collect. Throws if exceeded.
* Returns: {Promise\<Uint8Array>}

Collect all bytes from a stream into a single `Uint8Array`.

```mjs
import { from, bytes } from 'node:stream/new';

const data = await bytes(from('hello'));
console.log(data); // Uint8Array(5) [ 104, 101, 108, 108, 111 ]
```

```cjs
const { from, bytes } = require('node:stream/new');

async function run() {
  const data = await bytes(from('hello'));
  console.log(data); // Uint8Array(5) [ 104, 101, 108, 108, 111 ]
}

run().catch(console.error);
```

### `bytesSync(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `limit` {number}
* Returns: {Uint8Array}

Synchronous version of [`bytes()`][].

### `text(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable\<Uint8Array\[]>|Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `encoding` {string} Text encoding. **Default:** `'utf-8'`.
  * `signal` {AbortSignal}
  * `limit` {number}
* Returns: {Promise\<string>}

Collect all bytes and decode as text.

```mjs
import { from, text } from 'node:stream/new';

console.log(await text(from('hello'))); // 'hello'
```

```cjs
const { from, text } = require('node:stream/new');

async function run() {
  console.log(await text(from('hello'))); // 'hello'
}

run().catch(console.error);
```

### `textSync(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `encoding` {string} **Default:** `'utf-8'`.
  * `limit` {number}
* Returns: {string}

Synchronous version of [`text()`][].

## Utilities

### `merge(...sources[, options])`

<!-- YAML
added: REPLACEME
-->

* `...sources` {AsyncIterable\<Uint8Array\[]>} Two or more async iterables.
* `options` {Object}
  * `signal` {AbortSignal}
* Returns: {AsyncIterable\<Uint8Array\[]>}

Merge multiple async iterables by yielding batches in temporal order
(whichever source produces data first). All sources are consumed
concurrently.

```mjs
import { from, merge, text } from 'node:stream/new';

const merged = merge(from('hello '), from('world'));
console.log(await text(merged)); // Order depends on timing
```

```cjs
const { from, merge, text } = require('node:stream/new');

async function run() {
  const merged = merge(from('hello '), from('world'));
  console.log(await text(merged)); // Order depends on timing
}

run().catch(console.error);
```

### `ondrain(drainable)`

<!-- YAML
added: REPLACEME
-->

* `drainable` {Object} An object implementing the drainable protocol.
* Returns: {Promise\<boolean>|null}

Wait for a drainable writer's backpressure to clear. Returns a promise that
resolves to `true` when the writer can accept more data, or `null` if the
object does not implement the drainable protocol.

```mjs
import { push, ondrain, text } from 'node:stream/new';

const { writer, readable } = push({ highWaterMark: 2 });
writer.writeSync('a');
writer.writeSync('b');

// Start consuming so the buffer can actually drain
const consuming = text(readable);

// Buffer is full -- wait for drain
const canWrite = await ondrain(writer);
if (canWrite) {
  await writer.write('c');
}
await writer.end();
await consuming;
```

```cjs
const { push, ondrain, text } = require('node:stream/new');

async function run() {
  const { writer, readable } = push({ highWaterMark: 2 });
  writer.writeSync('a');
  writer.writeSync('b');

  // Start consuming so the buffer can actually drain
  const consuming = text(readable);

  // Buffer is full -- wait for drain
  const canWrite = await ondrain(writer);
  if (canWrite) {
    await writer.write('c');
  }
  await writer.end();
  await consuming;
}

run().catch(console.error);
```

### `tap(callback)`

<!-- YAML
added: REPLACEME
-->

* `callback` {Function} `(chunks) => void` Called with each batch.
* Returns: {Function} A stateless transform.

Create a pass-through transform that observes batches without modifying them.
Useful for logging, metrics, or debugging.

```mjs
import { from, pull, text, tap } from 'node:stream/new';

const result = pull(
  from('hello'),
  tap((chunks) => console.log('Batch size:', chunks.length)),
);
console.log(await text(result));
```

```cjs
const { from, pull, text, tap } = require('node:stream/new');

async function run() {
  const result = pull(
    from('hello'),
    tap((chunks) => console.log('Batch size:', chunks.length)),
  );
  console.log(await text(result));
}

run().catch(console.error);
```

### `tapSync(callback)`

<!-- YAML
added: REPLACEME
-->

* `callback` {Function}
* Returns: {Function}

Synchronous version of [`tap()`][].

## Multi-consumer

### `broadcast([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `highWaterMark` {number} Buffer size in slots. Must be >= 1; values
    below 1 are clamped to 1. **Default:** `16`.
  * `backpressure` {string} `'strict'` or `'block'`. **Default:** `'strict'`.
  * `signal` {AbortSignal}
* Returns: {Object}
  * `writer` {BroadcastWriter}
  * `broadcast` {Broadcast}

Create a push-model multi-consumer broadcast channel. A single writer pushes
data to multiple consumers. Each consumer has an independent cursor into a
shared buffer.

```mjs
import { broadcast, text } from 'node:stream/new';

const { writer, broadcast: bc } = broadcast();

// Create consumers before writing
const c1 = bc.push();  // Consumer 1
const c2 = bc.push();  // Consumer 2

// Producer and consumers must run concurrently. Awaited writes
// block when the buffer fills until consumers read.
const producing = (async () => {
  await writer.write('hello');
  await writer.end();
})();

const [r1, r2] = await Promise.all([text(c1), text(c2)]);
console.log(r1); // 'hello'
console.log(r2); // 'hello'
await producing;
```

```cjs
const { broadcast, text } = require('node:stream/new');

async function run() {
  const { writer, broadcast: bc } = broadcast();

  // Create consumers before writing
  const c1 = bc.push();  // Consumer 1
  const c2 = bc.push();  // Consumer 2

  // Producer and consumers must run concurrently. Awaited writes
  // block when the buffer fills until consumers read.
  const producing = (async () => {
    await writer.write('hello');
    await writer.end();
  })();

  const [r1, r2] = await Promise.all([text(c1), text(c2)]);
  console.log(r1); // 'hello'
  console.log(r2); // 'hello'
  await producing;
}

run().catch(console.error);
```

#### `broadcast.cancel([reason])`

* `reason` {Error}

Cancel the broadcast. All consumers receive an error.

#### `broadcast.push([...transforms][, options])`

* `...transforms` {Function|Object}
* `options` {Object}
  * `signal` {AbortSignal}
* Returns: {AsyncIterable\<Uint8Array\[]>}

Create a new consumer. Each consumer receives all data written to the
broadcast from the point of subscription onward. Optional transforms are
applied to this consumer's view of the data.

#### `broadcast[Symbol.dispose]()`

Alias for `broadcast.cancel()`.

### `Broadcast.from(input[, options])`

<!-- YAML
added: REPLACEME
-->

* `input` {AsyncIterable|Iterable|Broadcastable}
* `options` {Object} Same as `broadcast()`.
* Returns: {Object} `{ writer, broadcast }`

Create a broadcast from an existing source. The source is consumed
automatically and pushed to all subscribers.

### `share(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable} The source to share.
* `options` {Object}
  * `highWaterMark` {number} Buffer size. Must be >= 1; values below 1
    are clamped to 1. **Default:** `16`.
  * `backpressure` {string} `'strict'`, `'block'`, or `'drop-oldest'`.
    **Default:** `'strict'`.
* Returns: {Share}

Create a pull-model multi-consumer shared stream. Unlike `broadcast()`, the
source is only read when a consumer pulls. Multiple consumers share a single
buffer.

```mjs
import { from, share, text } from 'node:stream/new';

const shared = share(from('hello'));

const c1 = shared.pull();
const c2 = shared.pull();

console.log(await text(c1)); // 'hello'
console.log(await text(c2)); // 'hello'
```

```cjs
const { from, share, text } = require('node:stream/new');

async function run() {
  const shared = share(from('hello'));

  const c1 = shared.pull();
  const c2 = shared.pull();

  console.log(await text(c1)); // 'hello'
  console.log(await text(c2)); // 'hello'
}

run().catch(console.error);
```

#### `share.cancel([reason])`

* `reason` {Error}

Cancel the share. All consumers receive an error.

#### `share.pull([...transforms][, options])`

* `...transforms` {Function|Object}
* `options` {Object}
  * `signal` {AbortSignal}
* Returns: {AsyncIterable\<Uint8Array\[]>}

Create a new consumer of the shared source.

#### `share[Symbol.dispose]()`

Alias for `share.cancel()`.

### `Share.from(input[, options])`

<!-- YAML
added: REPLACEME
-->

* `input` {AsyncIterable|Shareable}
* `options` {Object} Same as `share()`.
* Returns: {Share}

Create a share from an existing source.

### `shareSync(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {Iterable} The sync source to share.
* `options` {Object}
  * `highWaterMark` {number} Must be >= 1; values below 1 are clamped
    to 1. **Default:** `16`.
  * `backpressure` {string} **Default:** `'strict'`.
* Returns: {SyncShare}

Synchronous version of [`share()`][].

### `SyncShare.fromSync(input[, options])`

<!-- YAML
added: REPLACEME
-->

* `input` {Iterable|SyncShareable}
* `options` {Object}
* Returns: {SyncShare}

## Compression and decompression

These transforms use the built-in zlib, Brotli, and Zstd compression
available in Node.js. Compression work is performed asynchronously,
overlapping with upstream I/O for maximum throughput.

All compression transforms are stateful (they return `{ transform }` objects)
and can be passed to `pull()`, `pipeTo()`, or `push()`.

### `compressBrotli([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `chunkSize` {number} **Default:** `16384`.
  * `params` {Object} Key-value object where keys and values are
    `zlib.constants` entries. The most important compressor parameters are:
    * `BROTLI_PARAM_MODE` -- `BROTLI_MODE_GENERIC` (default),
      `BROTLI_MODE_TEXT`, or `BROTLI_MODE_FONT`.
    * `BROTLI_PARAM_QUALITY` -- ranges from `BROTLI_MIN_QUALITY` to
      `BROTLI_MAX_QUALITY`. **Default:** `BROTLI_DEFAULT_QUALITY`.
    * `BROTLI_PARAM_SIZE_HINT` -- expected input size. **Default:** `0`
      (unknown).
    * `BROTLI_PARAM_LGWIN` -- window size (log2). Ranges from
      `BROTLI_MIN_WINDOW_BITS` to `BROTLI_MAX_WINDOW_BITS`.
    * `BROTLI_PARAM_LGBLOCK` -- input block size (log2).
      See the [Brotli compressor options][] in the zlib documentation for the
      full list.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a Brotli compression transform. Output is compatible with
`zlib.brotliDecompress()` and `decompressBrotli()`.

### `compressDeflate([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `16384`.
  * `level` {number} Compression level (`0`-`9`). **Default:** `Z_DEFAULT_COMPRESSION`.
  * `windowBits` {number} **Default:** `Z_DEFAULT_WINDOWBITS`.
  * `memLevel` {number} **Default:** `Z_DEFAULT_MEMLEVEL`.
  * `strategy` {number} **Default:** `Z_DEFAULT_STRATEGY`.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a deflate compression transform. Output is compatible with
`zlib.inflate()` and `decompressDeflate()`.

### `compressGzip([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `16384`.
  * `level` {number} Compression level (`0`-`9`). **Default:** `Z_DEFAULT_COMPRESSION`.
  * `windowBits` {number} **Default:** `Z_DEFAULT_WINDOWBITS`.
  * `memLevel` {number} **Default:** `Z_DEFAULT_MEMLEVEL`.
  * `strategy` {number} **Default:** `Z_DEFAULT_STRATEGY`.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a gzip compression transform. Output is compatible with `zlib.gunzip()`
and `decompressGzip()`.

```mjs
import { from, pull, bytes, text, compressGzip, decompressGzip } from 'node:stream/new';

const compressed = await bytes(pull(from('hello'), compressGzip()));
const original = await text(pull(from(compressed), decompressGzip()));
console.log(original); // 'hello'
```

```cjs
const { from, pull, bytes, text, compressGzip, decompressGzip } = require('node:stream/new');

async function run() {
  const compressed = await bytes(pull(from('hello'), compressGzip()));
  const original = await text(pull(from(compressed), decompressGzip()));
  console.log(original); // 'hello'
}

run().catch(console.error);
```

### `compressZstd([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `chunkSize` {number} **Default:** `16384`.
  * `params` {Object} Key-value object where keys and values are
    `zlib.constants` entries. The most important compressor parameters are:
    * `ZSTD_c_compressionLevel` -- **Default:** `ZSTD_CLEVEL_DEFAULT` (3).
    * `ZSTD_c_checksumFlag` -- generate a checksum. **Default:** `0`.
    * `ZSTD_c_strategy` -- compression strategy. Values include
      `ZSTD_fast`, `ZSTD_dfast`, `ZSTD_greedy`, `ZSTD_lazy`,
      `ZSTD_lazy2`, `ZSTD_btlazy2`, `ZSTD_btopt`, `ZSTD_btultra`,
      `ZSTD_btultra2`.
      See the [Zstd compressor options][] in the zlib documentation for the
      full list.
  * `pledgedSrcSize` {number} Expected uncompressed size (optional hint).
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a Zstandard compression transform. Output is compatible with
`zlib.zstdDecompress()` and `decompressZstd()`.

### `decompressBrotli([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `chunkSize` {number} **Default:** `16384`.
  * `params` {Object} Key-value object where keys and values are
    `zlib.constants` entries. Available decompressor parameters:
    * `BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION` -- boolean
      flag affecting internal memory allocation.
    * `BROTLI_DECODER_PARAM_LARGE_WINDOW` -- boolean flag enabling "Large
      Window Brotli" mode (not compatible with [RFC 7932][]).
      See the [Brotli decompressor options][] in the zlib documentation for
      details.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a Brotli decompression transform.

### `decompressDeflate([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `16384`.
  * `level` {number} Compression level (`0`-`9`). **Default:** `Z_DEFAULT_COMPRESSION`.
  * `windowBits` {number} **Default:** `Z_DEFAULT_WINDOWBITS`.
  * `memLevel` {number} **Default:** `Z_DEFAULT_MEMLEVEL`.
  * `strategy` {number} **Default:** `Z_DEFAULT_STRATEGY`.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a deflate decompression transform.

### `decompressGzip([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `16384`.
  * `level` {number} Compression level (`0`-`9`). **Default:** `Z_DEFAULT_COMPRESSION`.
  * `windowBits` {number} **Default:** `Z_DEFAULT_WINDOWBITS`.
  * `memLevel` {number} **Default:** `Z_DEFAULT_MEMLEVEL`.
  * `strategy` {number} **Default:** `Z_DEFAULT_STRATEGY`.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a gzip decompression transform.

### `decompressZstd([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `chunkSize` {number} **Default:** `16384`.
  * `params` {Object} Key-value object where keys and values are
    `zlib.constants` entries. Available decompressor parameters:
    * `ZSTD_d_windowLogMax` -- maximum window size (log2) the decompressor
      will allocate. Limits memory usage against malicious input.
      See the [Zstd decompressor options][] in the zlib documentation for
      details.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a Zstandard decompression transform.

## Protocol symbols

These well-known symbols allow third-party objects to participate in the
streaming protocol without importing from `node:stream/new` directly.

### `Stream.broadcastProtocol`

* Value: `Symbol.for('Stream.broadcastProtocol')`

Implement to make an object usable with `Broadcast.from()`.

### `Stream.drainableProtocol`

* Value: `Symbol.for('Stream.drainableProtocol')`

Implement to make a writer compatible with `ondrain()`. The method should
return a promise that resolves when backpressure clears, or `null` if no
backpressure.

### `Stream.shareProtocol`

* Value: `Symbol.for('Stream.shareProtocol')`

Implement to make an object usable with `Share.from()`.

### `Stream.shareSyncProtocol`

* Value: `Symbol.for('Stream.shareSyncProtocol')`

Implement to make an object usable with `SyncShare.fromSync()`.

### `Stream.toAsyncStreamable`

* Value: `Symbol.for('Stream.toAsyncStreamable')`

Async version of `toStreamable`. The method may return a promise.

### `Stream.toStreamable`

* Value: `Symbol.for('Stream.toStreamable')`

Implement this symbol as a method that returns a sync-streamable value
(string, `Uint8Array`, `Iterable`, etc.). Used by `from()` and `fromSync()`.

```js
const obj = {
  [Symbol.for('Stream.toStreamable')]() {
    return 'hello from custom object';
  },
};
// from(obj) and fromSync(obj) will UTF-8 encode the returned string.
```

[Brotli compressor options]: zlib.md#compressor-options
[Brotli decompressor options]: zlib.md#decompressor-options
[RFC 7932]: https://www.rfc-editor.org/rfc/rfc7932
[Zstd compressor options]: zlib.md#compressor-options-1
[Zstd decompressor options]: zlib.md#decompressor-options-1
[`array()`]: #arraysource-options
[`arrayBuffer()`]: #arraybuffersource-options
[`bytes()`]: #bytessource-options
[`from()`]: #frominput
[`pipeTo()`]: #pipetosource-transforms-writer-options
[`pull()`]: #pullsource-transforms-options
[`share()`]: #sharesource-options
[`tap()`]: #tapcallback
[`text()`]: #textsource-options
