// Flags: --experimental-stream-iter --expose-gc

import { mustCall } from '../common/index.mjs';
import { createRequire } from 'node:module';
import assert from 'node:assert';
import {
  array,
  dump,
  dumpSync,
  from,
  fromSync,
  pull,
  push,
  tap,
  toAsyncStreamable,
} from 'node:stream/iter';

const require = createRequire(import.meta.url);
const { gcUntil } = require('../common/gc');

// =============================================================================
// dump
// =============================================================================

async function testDumpResolvesUndefined() {
  assert.strictEqual(await dump(from('hello')), undefined);
}

async function testDumpConsumesToCompletion() {
  let pulls = 0;
  async function* gen() {
    for (let i = 0; i < 5; i++) {
      pulls++;
      yield [new Uint8Array([i])];
    }
  }
  await dump(gen());
  assert.strictEqual(pulls, 5);
}

async function testDumpEmpty() {
  assert.strictEqual(await dump(from([])), undefined);
  async function* empty() {}
  assert.strictEqual(await dump(empty()), undefined);
}

async function testDumpStringSource() {
  // Sources are normalized via from(), so raw ByteInput works.
  assert.strictEqual(await dump('a direct string'), undefined);
}

async function testDumpSyncSourceViaAsync() {
  // dump() accepts sync iterables too, like the other async consumers.
  assert.strictEqual(await dump(fromSync('sync source')), undefined);
}

async function testDumpAlreadyConsumed() {
  // A source that has already been read to completion drains to a no-op
  // rather than throwing.
  async function* gen() {
    yield [new Uint8Array([1])];
  }
  const source = gen();
  await dump(source);
  assert.strictEqual(await dump(source), undefined);
}

// =============================================================================
// dump: error propagation
// =============================================================================

async function testDumpRejectsOnMidStreamError() {
  const boom = new Error('boom');
  async function* failing() {
    yield [new Uint8Array([1])];
    yield [new Uint8Array([2])];
    throw boom;
  }
  await assert.rejects(dump(failing()), boom);
}

async function testDumpRejectsOnFirstChunkError() {
  const boom = new Error('immediate boom');
  async function* failing() {
    await Promise.reject(boom);
    yield [];
  }
  await assert.rejects(dump(failing()), boom);
}

async function testDumpRejectsInvalidChunkType() {
  async function* bad() {
    yield [42];
  }
  await assert.rejects(dump(bad()), { code: 'ERR_INVALID_ARG_TYPE' });
}

async function testDumpCallsReturnOnError() {
  // An abrupt completion must still release the source.
  let returned = false;
  const boom = new Error('cleanup boom');
  const source = {
    __proto__: null,
    async *[Symbol.asyncIterator]() {
      try {
        yield [new Uint8Array([1])];
        throw boom;
      } finally {
        returned = true;
      }
    },
  };
  await assert.rejects(dump(source), boom);
  assert.strictEqual(returned, true);
}

// =============================================================================
// dump: AbortSignal
// =============================================================================

async function testDumpAlreadyAbortedSignal() {
  await assert.rejects(
    () => dump(from('data'), { signal: AbortSignal.abort() }),
    { name: 'AbortError' },
  );
}

async function testDumpAbortsPendingNext() {
  const ac = new AbortController();
  const reason = new Error('dump abort');

  async function* never() {
    await new Promise(() => {});
    yield [];
  }

  const promise = dump(never(), { __proto__: null, signal: ac.signal });
  ac.abort(reason);

  await assert.rejects(promise, reason);
}

async function testDumpAbortsPendingNormalization() {
  const ac = new AbortController();
  const reason = new Error('dump normalization abort');
  const source = {
    __proto__: null,
    [toAsyncStreamable]() {
      return new Promise(() => {});
    },
  };

  const promise = dump(source, { __proto__: null, signal: ac.signal });
  ac.abort(reason);

  await assert.rejects(promise, reason);
}

async function testDumpUnabortedSignalCompletes() {
  const ac = new AbortController();
  assert.strictEqual(
    await dump(from('finishes first'), { signal: ac.signal }),
    undefined,
  );
}

// =============================================================================
// dump: limit
// =============================================================================

async function testDumpLimitRejects() {
  async function* gen() {
    yield [new Uint8Array(100)];
    yield [new Uint8Array(100)];
  }
  await assert.rejects(
    () => dump(gen(), { limit: 150 }),
    { name: 'RangeError', code: 'ERR_OUT_OF_RANGE' },
  );
}

async function testDumpLimitAllowsExact() {
  async function* gen() {
    yield [new Uint8Array(50)];
    yield [new Uint8Array(50)];
  }
  assert.strictEqual(await dump(gen(), { limit: 100 }), undefined);
}

async function testDumpLimitCancelsSource() {
  // Exceeding the limit is an abrupt completion, so the source must be
  // released rather than left half-read.
  let returned = false;
  const source = {
    __proto__: null,
    async *[Symbol.asyncIterator]() {
      try {
        while (true) yield [new Uint8Array(64)];
      } finally {
        returned = true;
      }
    },
  };
  await assert.rejects(() => dump(source, { limit: 128 }),
                       { code: 'ERR_OUT_OF_RANGE' });
  assert.strictEqual(returned, true);
}

async function testDumpLimitIsOptIn() {
  // No default limit: a source larger than undici's 128 KB dump() default
  // must still be read to completion.
  async function* big() {
    for (let i = 0; i < 64; i++) yield [new Uint8Array(8192)];
  }
  assert.strictEqual(await dump(big()), undefined);
}

// =============================================================================
// dump: does not retain data
// =============================================================================

async function testDumpDoesNotRetain() {
  // Allocate a distinct buffer per chunk and keep only weak references to
  // them. After the consumer finishes, anything it retained is still
  // reachable; anything it discarded is collectable.
  const kChunks = 64;

  async function measure(consumer, keepResult) {
    const refs = [];
    async function* source() {
      for (let i = 0; i < kChunks; i++) {
        const buf = new Uint8Array(64 * 1024);
        refs.push(new WeakRef(buf));
        yield [buf];
      }
    }
    const result = await consumer(source());
    let live = kChunks;
    const settled = await gcUntil('dump retention', () => {
      live = refs.filter((ref) => ref.deref() !== undefined).length;
      return live === 0;
    }).then(() => true, () => false);
    // Keep the collecting consumer's result reachable across the GC, so its
    // retention is what is being measured rather than the result being dropped.
    if (keepResult) assert.strictEqual(result.length, kChunks);
    return { settled, live };
  }

  // dump() keeps nothing, so every chunk becomes collectable.
  const drained = await measure(dump, false);
  assert.ok(drained.settled,
            `dump() retained ${drained.live}/${kChunks} chunks`);

  // Positive control: array() does retain, which proves the measurement above
  // is capable of observing retention rather than always passing.
  const collected = await measure(array, true);
  assert.ok(collected.live === kChunks,
            `array() should retain every chunk, kept ${collected.live}`);
}

async function testDumpReleasesBackpressure() {
  // Consuming is what returns budget to the writer. With a small budget the
  // writer blocks until dump() pulls, so this only completes if dump()
  // really is reading.
  const { writer, readable } = push({ budget: 16384 });
  const chunk = new Uint8Array(8192);

  const draining = dump(readable);

  for (let i = 0; i < 32; i++) {
    await writer.write(chunk);
  }
  await writer.end();

  assert.strictEqual(await draining, undefined);
}

// =============================================================================
// dump: interaction with transforms
// =============================================================================

async function testDumpWithTap() {
  // The documented pattern: observe via tap() while retaining nothing.
  let seenBytes = 0;
  let seenBatches = 0;
  const counter = tap((chunks) => {
    // tap() invokes the callback once more with null to signal end of stream.
    if (chunks === null) return;
    seenBatches++;
    for (const chunk of chunks) seenBytes += chunk.byteLength;
  });

  await dump(pull(from('hello world'), counter));
  assert.strictEqual(seenBytes, 11);
  assert.ok(seenBatches > 0);
}

async function testDumpPropagatesTransformError() {
  const boom = new Error('transform boom');
  const badTransform = () => { throw boom; };
  await assert.rejects(dump(pull(from('hello'), badTransform)), boom);
}

// =============================================================================
// dumpSync
// =============================================================================

function testDumpSyncBasic() {
  assert.strictEqual(dumpSync(fromSync('hello')), undefined);
}

function testDumpSyncConsumesToCompletion() {
  let pulls = 0;
  function* gen() {
    for (let i = 0; i < 5; i++) {
      pulls++;
      yield new Uint8Array([i]);
    }
  }
  dumpSync(gen());
  assert.strictEqual(pulls, 5);
}

function testDumpSyncEmpty() {
  assert.strictEqual(dumpSync(fromSync([])), undefined);
  function* empty() {}
  assert.strictEqual(dumpSync(empty()), undefined);
}

function testDumpSyncStringSource() {
  assert.strictEqual(dumpSync('direct sync string'), undefined);
}

function testDumpSyncThrowsMidStream() {
  const boom = new Error('sync boom');
  function* failing() {
    yield new Uint8Array([1]);
    throw boom;
  }
  assert.throws(() => dumpSync(failing()), boom);
}

function testDumpSyncRejectsAsyncSource() {
  // Same contract as the other *Sync consumers: an async-only source is a
  // type error, not a silent no-op.
  async function* asyncGen() {
    yield [new Uint8Array([1])];
  }
  assert.throws(() => dumpSync(asyncGen()),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => dumpSync(Promise.resolve('x')),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

function testDumpSyncLimit() {
  function* gen() {
    yield new Uint8Array(100);
    yield new Uint8Array(100);
  }
  assert.throws(
    () => dumpSync(gen(), { limit: 150 }),
    { name: 'RangeError', code: 'ERR_OUT_OF_RANGE' },
  );
  assert.strictEqual(dumpSync(gen(), { limit: 200 }), undefined);
}

// =============================================================================
// Option validation
// =============================================================================

async function testDumpOptionValidation() {
  await assert.rejects(() => dump(from('a'), 42),
                       { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(() => dump(from('a'), { signal: 'bad' }),
                       { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(() => dump(from('a'), { limit: 'bad' }),
                       { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(() => dump(from('a'), { limit: -1 }),
                       { code: 'ERR_OUT_OF_RANGE' });
}

function testDumpSyncOptionValidation() {
  assert.throws(() => dumpSync(fromSync('a'), 42),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => dumpSync(fromSync('a'), { limit: 'bad' }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => dumpSync(fromSync('a'), { limit: -1 }),
                { code: 'ERR_OUT_OF_RANGE' });
}

testDumpSyncBasic();
testDumpSyncConsumesToCompletion();
testDumpSyncEmpty();
testDumpSyncStringSource();
testDumpSyncThrowsMidStream();
testDumpSyncRejectsAsyncSource();
testDumpSyncLimit();
testDumpSyncOptionValidation();

await Promise.all([
  testDumpResolvesUndefined(),
  testDumpConsumesToCompletion(),
  testDumpEmpty(),
  testDumpStringSource(),
  testDumpSyncSourceViaAsync(),
  testDumpAlreadyConsumed(),
  testDumpRejectsOnMidStreamError(),
  testDumpRejectsOnFirstChunkError(),
  testDumpRejectsInvalidChunkType(),
  testDumpCallsReturnOnError(),
  testDumpAlreadyAbortedSignal(),
  testDumpAbortsPendingNext(),
  testDumpAbortsPendingNormalization(),
  testDumpUnabortedSignalCompletes(),
  testDumpLimitRejects(),
  testDumpLimitAllowsExact(),
  testDumpLimitCancelsSource(),
  testDumpLimitIsOptIn(),
  testDumpDoesNotRetain(),
  testDumpReleasesBackpressure(),
  testDumpWithTap(),
  testDumpPropagatesTransformError(),
  testDumpOptionValidation(),
]).then(mustCall());
