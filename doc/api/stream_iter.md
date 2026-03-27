# Iterable Streams

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

<!-- source_link=lib/stream/iter.js -->

The `node:stream/iter` module provides a streaming API built on iterables
rather than the event-driven `Readable`/`Writable`/`Transform` class hierarchy,
or the Web Streams `ReadableStream`/`WritableStream`/`TransformStream` interfaces.

This module is available only when the `--experimental-stream-iter` CLI flag
is enabled.

Streams are represented as `AsyncIterable<Uint8Array[]>` (async) or
`Iterable<Uint8Array[]>` (sync). There are no base classes to extend -- any
object implementing the iterable protocol can participate. Transforms are plain
functions or objects with a `transform` method.

Data flows in **batches** (`Uint8Array[]` per iteration) to amortize the cost
of async operations.

```mjs
import { from, pull, text } from 'node:stream/iter';
import { compressGzip, decompressGzip } from 'node:zlib/iter';

// Compress and decompress a string
const compressed = pull(from('Hello, world!'), compressGzip());
const result = await text(pull(compressed, decompressGzip()));
console.log(result); // 'Hello, world!'
```

```cjs
const { from, pull, text } = require('node:stream/iter');
const { compressGzip, decompressGzip } = require('node:zlib/iter');

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
import { text, pipeTo } from 'node:stream/iter';
import { compressGzip, decompressGzip } from 'node:zlib/iter';

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
const { text, pipeTo } = require('node:stream/iter');
const { compressGzip, decompressGzip } = require('node:zlib/iter');

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

All data in this API is represented as `Uint8Array` bytes. Strings
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
import { push, text } from 'node:stream/iter';

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
const { push, text } = require('node:stream/iter');

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
import { push, text } from 'node:stream/iter';

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
const { push, text } = require('node:stream/iter');

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
import { push } from 'node:stream/iter';

// Keep only the 5 most recent readings
const { writer, readable } = push({
  highWaterMark: 5,
  backpressure: 'drop-oldest',
});
```

```cjs
const { push } = require('node:stream/iter');

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
import { push } from 'node:stream/iter';

// Accept up to 10 buffered items; discard anything beyond that
const { writer, readable } = push({
  highWaterMark: 10,
  backpressure: 'drop-newest',
});
```

```cjs
const { push } = require('node:stream/iter');

// Accept up to 10 buffered items; discard anything beyond that
const { writer, readable } = push({
  highWaterMark: 10,
  backpressure: 'drop-newest',
});
```

### Writer interface

A writer is any object conforming to the Writer interface. Only `write()` is
required; all other methods are optional.

Each async method has a synchronous `*Sync` counterpart designed for a
try-fallback pattern: attempt the fast synchronous path first, and fall back
to the async version only when the synchronous call indicates it could not
complete:

```mjs
if (!writer.writeSync(chunk)) await writer.write(chunk);
if (!writer.writevSync(chunks)) await writer.writev(chunks);
if (writer.endSync() < 0) await writer.end();
writer.fail(err);  // Always synchronous, no fallback needed
```

### `writer.desiredSize`

* {number|null}

The number of buffer slots available before the high water mark is reached.
Returns `null` if the writer is closed or the consumer has disconnected.

The value is always non-negative.

### `writer.end([options])`

* `options` {Object}
  * `signal` {AbortSignal} Cancel just this operation. The signal cancels only
    the pending `end()` call; it does not fail the writer itself.
* Returns: {Promise\<number>} Total bytes written.

Signal that no more data will be written.

### `writer.endSync()`

* Returns: {number} Total bytes written, or `-1` if the writer is not open.

Synchronous variant of `writer.end()`. Returns `-1` if the writer is already
closed or errored. Can be used as a try-fallback pattern:

```cjs
const result = writer.endSync();
if (result < 0) {
  writer.end();
}
```

### `writer.fail(reason)`

* `reason` {any}

Put the writer into a terminal error state. If the writer is already closed
or errored, this is a no-op. Unlike `write()` and `end()`, `fail()` is
unconditionally synchronous because failing a writer is a pure state
transition with no async work to perform.

### `writer.write(chunk[, options])`

* `chunk` {Uint8Array|string}
* `options` {Object}
  * `signal` {AbortSignal} Cancel just this write operation. The signal cancels
    only the pending `write()` call; it does not fail the writer itself.
* Returns: {Promise\<void>}

Write a chunk. The promise resolves when buffer space is available.

### `writer.writeSync(chunk)`

* `chunk` {Uint8Array|string}
* Returns: {boolean} `true` if the write was accepted, `false` if the
  buffer is full.

Synchronous write. Does not block; returns `false` if backpressure is active.

### `writer.writev(chunks[, options])`

* `chunks` {Uint8Array\[]|string\[]}
* `options` {Object}
  * `signal` {AbortSignal} Cancel just this write operation. The signal cancels
    only the pending `writev()` call; it does not fail the writer itself.
* Returns: {Promise\<void>}

Write multiple chunks as a single batch.

### `writer.writevSync(chunks)`

* `chunks` {Uint8Array\[]|string\[]}
* Returns: {boolean} `true` if the write was accepted, `false` if the
  buffer is full.

Synchronous batch write.

## The `stream/iter` module

All functions are available both as named exports and as properties of the
`Stream` namespace object:

```mjs
// Named exports
import { from, pull, bytes, Stream } from 'node:stream/iter';

// Namespace access
Stream.from('hello');
```

```cjs
// Named exports
const { from, pull, bytes, Stream } = require('node:stream/iter');

// Namespace access
Stream.from('hello');
```

Including the `node:` prefix on the module specifier is optional.

## Sources

### `from(input)`

<!-- YAML
added: REPLACEME
-->

* `input` {string|ArrayBuffer|ArrayBufferView|Iterable|AsyncIterable|Object}
  Must not be `null` or `undefined`.
* Returns: {AsyncIterable\<Uint8Array\[]>}

Create an async byte stream from the given input. Strings are UTF-8 encoded.
`ArrayBuffer` and `ArrayBufferView` values are wrapped as `Uint8Array`. Arrays
and iterables are recursively flattened and normalized.

Objects implementing `Symbol.for('Stream.toAsyncStreamable')` or
`Symbol.for('Stream.toStreamable')` are converted via those protocols. The
`toAsyncStreamable` protocol takes precedence over `toStreamable`, which takes
precedence over the iteration protocols (`Symbol.asyncIterator`,
`Symbol.iterator`).

```mjs
import { Buffer } from 'node:buffer';
import { from, text } from 'node:stream/iter';

console.log(await text(from('hello')));       // 'hello'
console.log(await text(from(Buffer.from('hello')))); // 'hello'
```

```cjs
const { Buffer } = require('node:buffer');
const { from, text } = require('node:stream/iter');

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

* `input` {string|ArrayBuffer|ArrayBufferView|Iterable|Object}
  Must not be `null` or `undefined`.
* Returns: {Iterable\<Uint8Array\[]>}

Synchronous version of [`from()`][]. Returns a sync iterable. Cannot accept
async iterables or promises. Objects implementing
`Symbol.for('Stream.toStreamable')` are converted via that protocol (takes
precedence over `Symbol.iterator`). The `toAsyncStreamable` protocol is
ignored entirely.

```mjs
import { fromSync, textSync } from 'node:stream/iter';

console.log(textSync(fromSync('hello'))); // 'hello'
```

```cjs
const { fromSync, textSync } = require('node:stream/iter');

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

If the writer implements the optional `*Sync` methods (`writeSync`, `writevSync`,
`endSync`), `pipeTo()` will attempt to use the synchronous methods
first as a fast path, and fall back to the async versions only when the sync
methods indicate they cannot complete (e.g., backpressure or waiting for the
next tick). `fail()` is always called synchronously.

```mjs
import { from, pipeTo } from 'node:stream/iter';
import { compressGzip } from 'node:zlib/iter';
import { open } from 'node:fs/promises';

const fh = await open('output.gz', 'w');
const totalBytes = await pipeTo(
  from('Hello, world!'),
  compressGzip(),
  fh.writer({ autoClose: true }),
);
```

```cjs
const { from, pipeTo } = require('node:stream/iter');
const { compressGzip } = require('node:zlib/iter');
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

Synchronous version of [`pipeTo()`][]. The `source`, all transforms, and the
`writer` must be synchronous. Cannot accept async iterables or promises.

The `writer` must have the `*Sync` methods (`writeSync`, `writevSync`,
`endSync`) and `fail()` for this to work.

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
import { from, pull, text } from 'node:stream/iter';

const asciiUpper = (chunks) => {
  if (chunks === null) return null;
  return chunks.map((c) => {
    for (let i = 0; i < c.length; i++) {
      c[i] -= (c[i] >= 97 && c[i] <= 122) * 32;
    }
    return c;
  });
};

const result = pull(from('hello'), asciiUpper);
console.log(await text(result)); // 'HELLO'
```

```cjs
const { from, pull, text } = require('node:stream/iter');

const asciiUpper = (chunks) => {
  if (chunks === null) return null;
  return chunks.map((c) => {
    for (let i = 0; i < c.length; i++) {
      c[i] -= (c[i] >= 97 && c[i] <= 122) * 32;
    }
    return c;
  });
};

async function run() {
  const result = pull(from('hello'), asciiUpper);
  console.log(await text(result)); // 'HELLO'
}

run().catch(console.error);
```

Using an `AbortSignal`:

```mjs
import { pull } from 'node:stream/iter';

const ac = new AbortController();
const result = pull(source, transform, { signal: ac.signal });
ac.abort(); // Pipeline throws AbortError on next iteration
```

```cjs
const { pull } = require('node:stream/iter');

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
import { push, text } from 'node:stream/iter';

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
const { push, text } = require('node:stream/iter');

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

The writer returned by `push()` conforms to the \[Writer interface]\[].

## Duplex channels

### `duplex([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `highWaterMark` {number} Buffer size for both directions.
    **Default:** `4`.
  * `backpressure` {string} Policy for both directions.
    **Default:** `'strict'`.
  * `signal` {AbortSignal} Cancellation signal for both channels.
  * `a` {Object} Options specific to the A-to-B direction. Overrides
    shared options.
    * `highWaterMark` {number}
    * `backpressure` {string}
  * `b` {Object} Options specific to the B-to-A direction. Overrides
    shared options.
    * `highWaterMark` {number}
    * `backpressure` {string}
* Returns: {Array} A pair `[channelA, channelB]` of duplex channels.

Create a pair of connected duplex channels for bidirectional communication,
similar to `socketpair()`. Data written to one channel's writer appears in
the other channel's readable.

Each channel has:

* `writer` — a \[Writer interface]\[] object for sending data to the peer.
* `readable` — an `AsyncIterable<Uint8Array[]>` for reading data from
  the peer.
* `close()` — close this end of the channel (idempotent).
* `[Symbol.asyncDispose]()` — async dispose support for `await using`.

```mjs
import { duplex, text } from 'node:stream/iter';

const [client, server] = duplex();

// Server echoes back
const serving = (async () => {
  for await (const chunks of server.readable) {
    await server.writer.writev(chunks);
  }
})();

await client.writer.write('hello');
await client.writer.end();

console.log(await text(server.readable)); // handled by echo
await serving;
```

```cjs
const { duplex, text } = require('node:stream/iter');

async function run() {
  const [client, server] = duplex();

  // Server echoes back
  const serving = (async () => {
    for await (const chunks of server.readable) {
      await server.writer.writev(chunks);
    }
  })();

  await client.writer.write('hello');
  await client.writer.end();

  console.log(await text(server.readable)); // handled by echo
  await serving;
}

run().catch(console.error);
```

## Consumers

### `array(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable\<Uint8Array\[]>|Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `signal` {AbortSignal}
  * `limit` {number} Maximum number of bytes to consume. If the total bytes
    collected exceeds limit, an `ERR_OUT_OF_RANGE` error is thrown
* Returns: {Promise\<Uint8Array\[]>}

Collect all chunks as an array of `Uint8Array` values (without concatenating).

### `arrayBuffer(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable\<Uint8Array\[]>|Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `signal` {AbortSignal}
  * `limit` {number} Maximum number of bytes to consume. If the total bytes
    collected exceeds limit, an `ERR_OUT_OF_RANGE` error is thrown
* Returns: {Promise\<ArrayBuffer>}

Collect all bytes into an `ArrayBuffer`.

### `arrayBufferSync(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `limit` {number} Maximum number of bytes to consume. If the total bytes
    collected exceeds limit, an `ERR_OUT_OF_RANGE` error is thrown
* Returns: {ArrayBuffer}

Synchronous version of [`arrayBuffer()`][].

### `arraySync(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `limit` {number} Maximum number of bytes to consume. If the total bytes
    collected exceeds limit, an `ERR_OUT_OF_RANGE` error is thrown
* Returns: {Uint8Array\[]}

Synchronous version of [`array()`][].

### `bytes(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable\<Uint8Array\[]>|Iterable\<Uint8Array\[]>}
* `options` {Object}
  * `signal` {AbortSignal}
  * `limit` {number} Maximum number of bytes to consume. If the total bytes
    collected exceeds limit, an `ERR_OUT_OF_RANGE` error is thrown
* Returns: {Promise\<Uint8Array>}

Collect all bytes from a stream into a single `Uint8Array`.

```mjs
import { from, bytes } from 'node:stream/iter';

const data = await bytes(from('hello'));
console.log(data); // Uint8Array(5) [ 104, 101, 108, 108, 111 ]
```

```cjs
const { from, bytes } = require('node:stream/iter');

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
  * `limit` {number} Maximum number of bytes to consume. If the total bytes
    collected exceeds limit, an `ERR_OUT_OF_RANGE` error is thrown
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
  * `limit` {number} Maximum number of bytes to consume. If the total bytes
    collected exceeds limit, an `ERR_OUT_OF_RANGE` error is thrown
* Returns: {Promise\<string>}

Collect all bytes and decode as text.

```mjs
import { from, text } from 'node:stream/iter';

console.log(await text(from('hello'))); // 'hello'
```

```cjs
const { from, text } = require('node:stream/iter');

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
  * `limit` {number} Maximum number of bytes to consume. If the total bytes
    collected exceeds limit, an `ERR_OUT_OF_RANGE` error is thrown
* Returns: {string}

Synchronous version of [`text()`][].

## Utilities

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
import { push, ondrain, text } from 'node:stream/iter';

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
const { push, ondrain, text } = require('node:stream/iter');

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

### `merge(...sources[, options])`

<!-- YAML
added: REPLACEME
-->

* `...sources` {AsyncIterable\<Uint8Array\[]>|Iterable\<Uint8Array\[]>} Two or more iterables.
* `options` {Object}
  * `signal` {AbortSignal}
* Returns: {AsyncIterable\<Uint8Array\[]>}

Merge multiple async iterables by yielding batches in temporal order
(whichever source produces data first). All sources are consumed
concurrently.

```mjs
import { from, merge, text } from 'node:stream/iter';

const merged = merge(from('hello '), from('world'));
console.log(await text(merged)); // Order depends on timing
```

```cjs
const { from, merge, text } = require('node:stream/iter');

async function run() {
  const merged = merge(from('hello '), from('world'));
  console.log(await text(merged)); // Order depends on timing
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
import { from, pull, text, tap } from 'node:stream/iter';

const result = pull(
  from('hello'),
  tap((chunks) => console.log('Batch size:', chunks.length)),
);
console.log(await text(result));
```

```cjs
const { from, pull, text, tap } = require('node:stream/iter');

async function run() {
  const result = pull(
    from('hello'),
    tap((chunks) => console.log('Batch size:', chunks.length)),
  );
  console.log(await text(result));
}

run().catch(console.error);
```

`tap()` intentionally does not prevent in-place modification of the
chunks by the tapping callback; but return values are ignored.

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
  * `backpressure` {string} `'strict'`, `'block'`, `'drop-oldest'`, or
    `'drop-newest'`. **Default:** `'strict'`.
  * `signal` {AbortSignal}
* Returns: {Object}
  * `writer` {BroadcastWriter}
  * `broadcast` {Broadcast}

Create a push-model multi-consumer broadcast channel. A single writer pushes
data to multiple consumers. Each consumer has an independent cursor into a
shared buffer.

```mjs
import { broadcast, text } from 'node:stream/iter';

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
const { broadcast, text } = require('node:stream/iter');

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

#### `broadcast.bufferSize`

* {number}

The number of chunks currently buffered.

#### `broadcast.cancel([reason])`

* `reason` {Error}

Cancel the broadcast. All consumers receive an error.

#### `broadcast.consumerCount`

* {number}

The number of active consumers.

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

Create a {Broadcast} from an existing source. The source is consumed
automatically and pushed to all subscribers.

### `share(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {AsyncIterable} The source to share.
* `options` {Object}
  * `highWaterMark` {number} Buffer size. Must be >= 1; values below 1
    are clamped to 1. **Default:** `16`.
  * `backpressure` {string} `'strict'`, `'block'`, `'drop-oldest'`, or
    `'drop-newest'`. **Default:** `'strict'`.
* Returns: {Share}

Create a pull-model multi-consumer shared stream. Unlike `broadcast()`, the
source is only read when a consumer pulls. Multiple consumers share a single
buffer.

```mjs
import { from, share, text } from 'node:stream/iter';

const shared = share(from('hello'));

const c1 = shared.pull();
const c2 = shared.pull();

// Consume concurrently to avoid deadlock with small buffers.
const [r1, r2] = await Promise.all([text(c1), text(c2)]);
console.log(r1); // 'hello'
console.log(r2); // 'hello'
```

```cjs
const { from, share, text } = require('node:stream/iter');

async function run() {
  const shared = share(from('hello'));

  const c1 = shared.pull();
  const c2 = shared.pull();

  // Consume concurrently to avoid deadlock with small buffers.
  const [r1, r2] = await Promise.all([text(c1), text(c2)]);
  console.log(r1); // 'hello'
  console.log(r2); // 'hello'
}

run().catch(console.error);
```

#### `share.bufferSize`

* {number}

The number of chunks currently buffered.

#### `share.cancel([reason])`

* `reason` {Error}

Cancel the share. All consumers receive an error.

#### `share.consumerCount`

* {number}

The number of active consumers.

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

Create a {Share} from an existing source.

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

## Compression and decompression transforms

Compression and decompression transforms for use with `pull()`, `pullSync()`,
`pipeTo()`, and `pipeToSync()` are available via the [`node:zlib/iter`][]
module. See the [`node:zlib/iter` documentation][] for details.

## Protocol symbols

These well-known symbols allow third-party objects to participate in the
streaming protocol without importing from `node:stream/iter` directly.

### `Stream.broadcastProtocol`

* Value: `Symbol.for('Stream.broadcastProtocol')`

The value must be a function. When called by `Broadcast.from()`, it receives
the options passed to `Broadcast.from()` and must return an object conforming
to the {Broadcast} interface. The implementation is fully custom -- it can
manage consumers, buffering, and backpressure however it wants.

```mjs
import { Broadcast, text } from 'node:stream/iter';

// This example defers to the built-in Broadcast, but a custom
// implementation could use any mechanism.
class MessageBus {
  #broadcast;
  #writer;

  constructor() {
    const { writer, broadcast } = Broadcast();
    this.#writer = writer;
    this.#broadcast = broadcast;
  }

  [Symbol.for('Stream.broadcastProtocol')](options) {
    return this.#broadcast;
  }

  send(data) {
    this.#writer.write(new TextEncoder().encode(data));
  }

  close() {
    this.#writer.end();
  }
}

const bus = new MessageBus();
const { broadcast } = Broadcast.from(bus);
const consumer = broadcast.push();
bus.send('hello');
bus.close();
console.log(await text(consumer)); // 'hello'
```

```cjs
const { Broadcast, text } = require('node:stream/iter');

// This example defers to the built-in Broadcast, but a custom
// implementation could use any mechanism.
class MessageBus {
  #broadcast;
  #writer;

  constructor() {
    const { writer, broadcast } = Broadcast();
    this.#writer = writer;
    this.#broadcast = broadcast;
  }

  [Symbol.for('Stream.broadcastProtocol')](options) {
    return this.#broadcast;
  }

  send(data) {
    this.#writer.write(new TextEncoder().encode(data));
  }

  close() {
    this.#writer.end();
  }
}

const bus = new MessageBus();
const { broadcast } = Broadcast.from(bus);
const consumer = broadcast.push();
bus.send('hello');
bus.close();
text(consumer).then(console.log); // 'hello'
```

### `Stream.drainableProtocol`

* Value: `Symbol.for('Stream.drainableProtocol')`

Implement to make a writer compatible with `ondrain()`. The method should
return a promise that resolves when backpressure clears, or `null` if no
backpressure.

```mjs
import { ondrain } from 'node:stream/iter';

class CustomWriter {
  #queue = [];
  #drain = null;
  #closed = false;
  [Symbol.for('Stream.drainableProtocol')]() {
    if (this.#closed) return null;
    if (this.#queue.length < 3) return Promise.resolve(true);
    this.#drain ??= Promise.withResolvers();
    return this.#drain.promise;
  }
  write(chunk) {
    this.#queue.push(chunk);
  }
  flush() {
    this.#queue.length = 0;
    this.#drain?.resolve(true);
    this.#drain = null;
  }
  close() {
    this.#closed = true;
  }
}
const writer = new CustomWriter();
const ready = ondrain(writer);
console.log(ready); // Promise { true } -- no backpressure
```

```cjs
const { ondrain } = require('node:stream/iter');

class CustomWriter {
  #queue = [];
  #drain = null;
  #closed = false;

  [Symbol.for('Stream.drainableProtocol')]() {
    if (this.#closed) return null;
    if (this.#queue.length < 3) return Promise.resolve(true);
    this.#drain ??= Promise.withResolvers();
    return this.#drain.promise;
  }

  write(chunk) {
    this.#queue.push(chunk);
  }

  flush() {
    this.#queue.length = 0;
    this.#drain?.resolve(true);
    this.#drain = null;
  }

  close() {
    this.#closed = true;
  }
}

const writer = new CustomWriter();
const ready = ondrain(writer);
console.log(ready); // Promise { true } -- no backpressure
```

### `Stream.shareProtocol`

* Value: `Symbol.for('Stream.shareProtocol')`

The value must be a function. When called by `Share.from()`, it receives the
options passed to `Share.from()` and must return an object conforming the the
{Share} interface. The implementation is fully custom -- it can manage the shared
source, consumers, buffering, and backpressure however it wants.

```mjs
import { share, Share, text } from 'node:stream/iter';

// This example defers to the built-in share(), but a custom
// implementation could use any mechanism.
class DataPool {
  #share;

  constructor(source) {
    this.#share = share(source);
  }

  [Symbol.for('Stream.shareProtocol')](options) {
    return this.#share;
  }
}

const pool = new DataPool(
  (async function* () {
    yield 'hello';
  })(),
);

const shared = Share.from(pool);
const consumer = shared.pull();
console.log(await text(consumer)); // 'hello'
```

```cjs
const { share, Share, text } = require('node:stream/iter');

// This example defers to the built-in share(), but a custom
// implementation could use any mechanism.
class DataPool {
  #share;

  constructor(source) {
    this.#share = share(source);
  }

  [Symbol.for('Stream.shareProtocol')](options) {
    return this.#share;
  }
}

const pool = new DataPool(
  (async function* () {
    yield 'hello';
  })(),
);

const shared = Share.from(pool);
const consumer = shared.pull();
text(consumer).then(console.log); // 'hello'
```

### `Stream.shareSyncProtocol`

* Value: `Symbol.for('Stream.shareSyncProtocol')`

The value must be a function. When called by `SyncShare.fromSync()`, it receives
the options passed to `SyncShare.fromSync()` and must return an object conforming
to the {SyncShare} interface. The implementation is fully custom -- it can manage
the shared source, consumers, and buffering however it wants.

```mjs
import { shareSync, SyncShare, textSync } from 'node:stream/iter';

// This example defers to the built-in shareSync(), but a custom
// implementation could use any mechanism.
class SyncDataPool {
  #share;

  constructor(source) {
    this.#share = shareSync(source);
  }

  [Symbol.for('Stream.shareSyncProtocol')](options) {
    return this.#share;
  }
}

const encoder = new TextEncoder();
const pool = new SyncDataPool(
  function* () {
    yield [encoder.encode('hello')];
  }(),
);

const shared = SyncShare.fromSync(pool);
const consumer = shared.pull();
console.log(textSync(consumer)); // 'hello'
```

```cjs
const { shareSync, SyncShare, textSync } = require('node:stream/iter');

// This example defers to the built-in shareSync(), but a custom
// implementation could use any mechanism.
class SyncDataPool {
  #share;

  constructor(source) {
    this.#share = shareSync(source);
  }

  [Symbol.for('Stream.shareSyncProtocol')](options) {
    return this.#share;
  }
}

const encoder = new TextEncoder();
const pool = new SyncDataPool(
  function* () {
    yield [encoder.encode('hello')];
  }(),
);

const shared = SyncShare.fromSync(pool);
const consumer = shared.pull();
console.log(textSync(consumer)); // 'hello'
```

### `Stream.toAsyncStreamable`

* Value: `Symbol.for('Stream.toAsyncStreamable')`

The value must be a function that converts the object into a streamable value.
When the object is encountered anywhere in the streaming pipeline (as a source
passed to `from()`, or as a value returned from a transform), this method is
called to produce the actual data. It may return (or resolve to) any streamable
value: a string, `Uint8Array`, `AsyncIterable`, `Iterable`, or another streamable
object.

```mjs
import { from, text } from 'node:stream/iter';

class Greeting {
  #name;

  constructor(name) {
    this.#name = name;
  }

  [Symbol.for('Stream.toAsyncStreamable')]() {
    return `hello ${this.#name}`;
  }
}

const stream = from(new Greeting('world'));
console.log(await text(stream)); // 'hello world'
```

```cjs
const { from, text } = require('node:stream/iter');

class Greeting {
  #name;

  constructor(name) {
    this.#name = name;
  }

  [Symbol.for('Stream.toAsyncStreamable')]() {
    return `hello ${this.#name}`;
  }
}

const stream = from(new Greeting('world'));
text(stream).then(console.log); // 'hello world'
```

### `Stream.toStreamable`

* Value: `Symbol.for('Stream.toStreamable')`

The value must be a function that synchronously converts the object into a
streamable value. When the object is encountered anywhere in the streaming
pipeline (as a source passed to `fromSync()`, or as a value returned from a
sync transform), this method is called to produce the actual data. It must
synchronously return a streamable value: a string, `Uint8Array`, or `Iterable`.

```mjs
import { fromSync, textSync } from 'node:stream/iter';

class Greeting {
  #name;

  constructor(name) {
    this.#name = name;
  }

  [Symbol.for('Stream.toStreamable')]() {
    return `hello ${this.#name}`;
  }
}

const stream = fromSync(new Greeting('world'));
console.log(textSync(stream)); // 'hello world'
```

```cjs
const { fromSync, textSync } = require('node:stream/iter');

class Greeting {
  #name;

  constructor(name) {
    this.#name = name;
  }

  [Symbol.for('Stream.toStreamable')]() {
    return `hello ${this.#name}`;
  }
}

const stream = fromSync(new Greeting('world'));
console.log(textSync(stream)); // 'hello world'
```

[`array()`]: #arraysource-options
[`arrayBuffer()`]: #arraybuffersource-options
[`bytes()`]: #bytessource-options
[`from()`]: #frominput
[`node:zlib/iter`]: zlib_iter.md
[`node:zlib/iter` documentation]: zlib_iter.md
[`pipeTo()`]: #pipetosource-transforms-writer-options
[`pull()`]: #pullsource-transforms-options
[`share()`]: #sharesource-options
[`tap()`]: #tapcallback
[`text()`]: #textsource-options
