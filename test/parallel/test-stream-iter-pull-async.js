// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  broadcast,
  dump,
  from,
  pipeTo,
  pull,
  push,
  share,
  tap,
  text,
  toAsyncStreamable,
} = require('stream/iter');

const { setImmediate } = require('timers/promises');
const { inspect } = require('util');
const { getEventListeners } = require('events');

async function testPullIdentity() {
  const data = await text(pull(from('hello-async')));
  assert.strictEqual(data, 'hello-async');
}

async function testPullStatelessTransform() {
  const upper = (chunks) => {
    if (chunks === null) return null;
    return chunks.map((c) => {
      const str = new TextDecoder().decode(c);
      return new TextEncoder().encode(str.toUpperCase());
    });
  };
  const data = await text(pull(from('abc'), upper));
  assert.strictEqual(data, 'ABC');
}

async function testPullStatefulTransform() {
  const stateful = {
    transform: async function*(source) {
      for await (const chunks of source) {
        if (chunks === null) {
          yield new TextEncoder().encode('-ASYNC-END');
          continue;
        }
        for (const chunk of chunks) {
          yield chunk;
        }
      }
    },
  };
  const data = await text(pull(from('data'), stateful));
  assert.strictEqual(data, 'data-ASYNC-END');
}

async function testPullStatefulTransformReceiver() {
  const descriptor = {};
  descriptor.transform = common.mustCall(
    async function*(source) {
      assert.strictEqual(this, descriptor);
      for await (const chunks of source) {
        yield chunks;
      }
    });

  assert.strictEqual(await text(pull(from('receiver'), descriptor)), 'receiver');
}

async function testPullWithAbortSignal() {
  let started = false;
  async function* gen() {
    started = true;
    yield [new Uint8Array([1])];
  }

  // An already-aborted signal does not make pull() throw; the returned
  // iterable rejects instead, without reading from the source.
  const signal = AbortSignal.abort();
  const iterator = pull(gen(), { signal })[Symbol.asyncIterator]();
  await assert.rejects(iterator.next(), (error) => error === signal.reason);
  await assert.rejects(iterator.next(), (error) => error === signal.reason);
  assert.strictEqual(started, false);
  assert.deepStrictEqual({ ...await iterator.return() },
                         { done: true, value: undefined });

  await assert.rejects(text(pull(gen(), (chunks) => chunks, { signal })),
                       (error) => error === signal.reason);
  assert.strictEqual(started, false);
}

async function testPullKeepsRejectingAfterAbort() {
  for (const transforms of [[], [(chunks) => chunks]]) {
    // Abort while a read is pending.
    {
      const ac = new AbortController();
      const reason = new Error('stop');
      async function* gen() {
        yield [new Uint8Array([1])];
        await new Promise(() => {});
      }
      const iterator =
        pull(gen(), ...transforms, { signal: ac.signal })[Symbol.asyncIterator]();
      assert.strictEqual((await iterator.next()).done, false);
      const pending = iterator.next();
      ac.abort(reason);
      await assert.rejects(pending, (error) => error === reason);
      await assert.rejects(iterator.next(), (error) => error === reason);
      await assert.rejects(iterator.next(), (error) => error === reason);
    }
    // Abort between reads.
    {
      const ac = new AbortController();
      const reason = new Error('stop');
      async function* gen() {
        yield [new Uint8Array([1])];
        yield [new Uint8Array([2])];
      }
      const iterator =
        pull(gen(), ...transforms, { signal: ac.signal })[Symbol.asyncIterator]();
      assert.strictEqual((await iterator.next()).done, false);
      ac.abort(reason);
      await assert.rejects(iterator.next(), (error) => error === reason);
      await assert.rejects(iterator.next(), (error) => error === reason);
    }
    // Aborting after the pipeline completed does not change the result.
    {
      const ac = new AbortController();
      const iterator =
        pull(from('x'), ...transforms, { signal: ac.signal })[Symbol.asyncIterator]();
      assert.strictEqual((await iterator.next()).done, false);
      assert.strictEqual((await iterator.next()).done, true);
      ac.abort();
      assert.strictEqual((await iterator.next()).done, true);
    }
  }
}

async function testPullNormalizesSourceAtCallTime() {
  let protocolCalls = 0;
  let iteratorCalls = 0;
  const source = {
    [toAsyncStreamable]() {
      protocolCalls++;
      return {
        async *[Symbol.asyncIterator]() {
          iteratorCalls++;
          yield 'data';
        },
      };
    },
  };

  const result = pull(source);
  assert.strictEqual(protocolCalls, 1);
  assert.strictEqual(iteratorCalls, 0);
  assert.strictEqual(await text(result), 'data');
  assert.strictEqual(protocolCalls, 1);
  assert.strictEqual(iteratorCalls, 1);
}

async function testPullPreAbortOrdering() {
  const reason = new Error('already aborted');
  let protocolCalls = 0;
  const source = {
    [toAsyncStreamable]() {
      protocolCalls++;
      return from('data');
    },
  };
  const signal = AbortSignal.abort(reason);

  // Source conversion still happens when pull() is called; the abort is
  // reported when the result is read.
  const result = pull(source, { signal });
  assert.strictEqual(protocolCalls, 1);
  await assert.rejects(text(result), (error) => error === reason);
  assert.throws(
    () => pull(null, { signal }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

async function testPullChainedTransforms() {
  const enc = new TextEncoder();
  const transforms = [
    (chunks) => {
      if (chunks === null) return null;
      return [...chunks, enc.encode('!')];
    },
    (chunks) => {
      if (chunks === null) return null;
      return [...chunks, enc.encode('?')];
    },
  ];
  const data = await text(pull(from('hello'), ...transforms));
  assert.strictEqual(data, 'hello!?');
}

// Source error → controller.abort() → transform listener throws →
// source error propagates to consumer; listener error becomes uncaught
// exception (per EventTarget spec behavior).
async function testTransformSignalListenerErrorOnSourceError() {
  // Listener errors from dispatchEvent are rethrown via process.nextTick,
  // so we must catch them as uncaught exceptions.
  const uncaughtErrors = [];
  const handler = (err) => uncaughtErrors.push(err);
  process.on('uncaughtException', handler);

  const throwingTransform = {
    transform(source, options) {
      options.signal.addEventListener('abort', () => {
        throw new Error('listener boom');
      });
      return source;
    },
  };

  async function* failingSource() {
    yield [new TextEncoder().encode('a')];
    throw new Error('source error');
  }

  await assert.rejects(
    async () => {
      await dump(pull(failingSource(), throwingTransform));
    },
    { message: 'source error' },
  );

  // Give the nextTick rethrow a chance to fire
  await setImmediate();
  process.removeListener('uncaughtException', handler);

  assert.strictEqual(uncaughtErrors.length, 1);
  assert.strictEqual(uncaughtErrors[0].message, 'listener boom');
}

// Pull source error propagates to consumer
async function testPullSourceError() {
  async function* failingSource() {
    yield [new TextEncoder().encode('a')];
    throw new Error('source boom');
  }
  await assert.rejects(async () => {
    await dump(pull(failingSource()));
  }, { message: 'source boom' });
}

// Tap callback error propagates through pipeline
async function testTapCallbackError() {
  const badTap = tap(() => { throw new Error('tap boom'); });
  await assert.rejects(async () => {
    await dump(pull(from('hello'), badTap));
  }, { message: 'tap boom' });
}

// Pull signal aborted mid-iteration (not pre-aborted)
async function testPullSignalAbortMidIteration() {
  const ac = new AbortController();
  const enc = new TextEncoder();
  async function* slowSource() {
    yield [enc.encode('a')];
    yield [enc.encode('b')];
    yield [enc.encode('c')];
  }
  const result = pull(slowSource(), { signal: ac.signal });
  const iter = result[Symbol.asyncIterator]();
  const first = await iter.next(); // Read first batch
  assert.strictEqual(first.done, false);
  ac.abort();
  await assert.rejects(() => iter.next(), { name: 'AbortError' });
}

async function testPullSignalAbortWhileSourceNextPending() {
  const source = {
    [Symbol.asyncIterator]() {
      return {
        async next() {
          await new Promise(() => {});
        },
      };
    },
  };
  const ac = new AbortController();
  const iter = pull(source, { signal: ac.signal })[Symbol.asyncIterator]();
  const next = iter.next();
  ac.abort();
  await assert.rejects(next, { name: 'AbortError' });
}

async function testPullReturnWhileSourceNextPending() {
  let startNext;
  const nextStarted = new Promise((resolve) => { startNext = resolve; });
  const source = {
    [Symbol.asyncIterator]() {
      return {
        next() {
          startNext();
          return new Promise(() => {});
        },
      };
    },
  };

  const iter = pull(source)[Symbol.asyncIterator]();
  const next = assert.rejects(iter.next(), { name: 'AbortError' });
  await nextStarted;

  const timeout = {};
  const result = await Promise.race([
    iter.return(),
    setImmediate(timeout),
  ]);

  assert.notStrictEqual(result, timeout);
  assert.deepStrictEqual({ ...result }, { value: undefined, done: true });
  await next;
}

async function testTransformedConsumerReturnBeforeNext() {
  const identity = (chunks) => chunks;
  const pushed = push(identity);
  const { broadcast: bc } = broadcast();
  const broadcastConsumer = bc.push(identity);
  const shared = share(from('shared'));
  const sharedConsumer = shared.pull(identity);

  assert.strictEqual(bc.consumerCount, 1);
  assert.strictEqual(shared.consumerCount, 1);

  const cases = [
    [pushed.readable, common.mustCall(
      () => assert.strictEqual(pushed.writer.canWrite, null))],
    [broadcastConsumer, common.mustCall(
      () => assert.strictEqual(bc.consumerCount, 0))],
    [sharedConsumer, common.mustCall(
      () => assert.strictEqual(shared.consumerCount, 0))],
  ];

  for (const [readable, verify] of cases) {
    await readable[Symbol.asyncIterator]().return();
    verify();
  }
}

async function testPullSignalAbortWithTransformWhileSourceNextPending() {
  const source = {
    [Symbol.asyncIterator]() {
      return {
        async next() {
          await new Promise(() => {});
        },
      };
    },
  };
  const ac = new AbortController();
  const iter = pull(
    source,
    (chunks) => chunks,
    { signal: ac.signal },
  )[Symbol.asyncIterator]();
  const next = iter.next();
  ac.abort();
  await assert.rejects(next, { name: 'AbortError' });
}

// An abort rejects a pending pull at once, wherever the pipeline is waiting:
// here, on a transform that never settles.
async function testPullSignalAbortWhileTransformPending() {
  const ac = new AbortController();
  let transformSignal;
  const iter = pull(from('a'), (chunks, options) => {
    transformSignal = options.signal;
    return new Promise(() => {});
  }, { signal: ac.signal })[Symbol.asyncIterator]();
  const next = iter.next();
  await setImmediate();
  const reason = new Error('stop');
  ac.abort(reason);
  await assert.rejects(next, reason);
  assert.strictEqual(transformSignal.reason, reason);
  await assert.rejects(iter.next(), reason);
}

// When the signal aborts while no pull is pending, the source is closed once,
// and the next pull rejects.
async function testPullSignalAbortWhileIdleClosesSource() {
  const log = [];
  const ac = new AbortController();
  const iter = pull(createLoggedSource(log, [1, 2, 3]), (chunks) => chunks,
                    { signal: ac.signal })[Symbol.asyncIterator]();
  assert.strictEqual((await iter.next()).done, false);
  const reason = new Error('stop');
  ac.abort(reason);
  await setImmediate();
  assert.deepStrictEqual(log, ['next 0', 'return']);
  await assert.rejects(iter.next(), reason);
  assert.strictEqual((await iter.return()).done, true);
  assert.deepStrictEqual(log, ['next 0', 'return']);
}

// The pipeline's listener on the signal is removed once the pipeline is
// done, however it ends.
async function testPullSignalListenerRemoved() {
  const identity = (chunks) => chunks;
  const ac = new AbortController();
  const { signal } = ac;
  await text(pull(from(['a', 'b']), identity, { signal }));
  assert.strictEqual(getEventListeners(signal, 'abort').length, 0);

  let iter = pull(from(['a', 'b']), identity, { signal })[Symbol.asyncIterator]();
  await iter.next();
  assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
  await iter.return();
  assert.strictEqual(getEventListeners(signal, 'abort').length, 0);

  iter = pull(from(['a', 'b']), () => { throw new Error('failed'); },
              { signal })[Symbol.asyncIterator]();
  await assert.rejects(iter.next(), /failed/);
  assert.strictEqual(getEventListeners(signal, 'abort').length, 0);

  iter = pull(from(['a', 'b']), identity, { signal })[Symbol.asyncIterator]();
  await iter.next();
  ac.abort();
  assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
  await assert.rejects(iter.next(), { name: 'AbortError' });
}

// Pull consumer break (return()) cleans up transform signal
async function testPullConsumerBreakCleanup() {
  let signalAborted = false;
  const trackingTransform = {
    transform(source, options) {
      options.signal.addEventListener('abort', () => {
        signalAborted = true;
      });
      return source;
    },
  };
  async function* infiniteSource() {
    let i = 0;
    while (true) {
      yield [new TextEncoder().encode(`chunk${i++}`)];
    }
  }
  // Consumer breaks after first chunk
  // eslint-disable-next-line no-unused-vars
  for await (const _ of pull(infiniteSource(), trackingTransform)) {
    break;
  }
  // Give the abort handler a tick to fire
  await setImmediate();
  assert.strictEqual(signalAborted, true);
}

// Pull transform returning a Promise
async function testPullTransformReturnsPromise() {
  const asyncTransform = async (chunks) => {
    if (chunks === null) return null;
    return chunks;
  };
  const result = await text(pull(from('hello'), asyncTransform));
  assert.strictEqual(result, 'hello');
}

// Stateless transform error propagates
async function testPullStatelessTransformError() {
  const badTransform = (chunks) => {
    if (chunks === null) return null;
    throw new Error('async stateless boom');
  };
  await assert.rejects(async () => {
    await dump(pull(from('hello'), badTransform));
  }, { message: 'async stateless boom' });
}

// Stateful transform error propagates
async function testPullStatefulTransformError() {
  const badStateful = {
    transform: async function*(source) { // eslint-disable-line require-yield
      for await (const chunks of source) {
        if (chunks === null) continue;
        throw new Error('async stateful boom');
      }
    },
  };
  await assert.rejects(async () => {
    await dump(pull(from('hello'), badStateful));
  }, { message: 'async stateful boom' });
}

// Stateless transform flush emitting data
async function testPullStatelessTransformFlush() {
  const withTrailer = (chunks) => {
    if (chunks === null) {
      return [new TextEncoder().encode('-TRAILER')];
    }
    return chunks;
  };
  const data = await text(pull(from('data'), withTrailer));
  assert.strictEqual(data, 'data-TRAILER');
}

// Consecutive stateless transforms each receive a final flush signal after
// upstream flush output has been processed.
async function testPullConsecutiveStatelessTransformFlush() {
  const enc = new TextEncoder();
  const addAOnFlush = (chunks) => (chunks === null ?
    [enc.encode('-A')] : chunks);
  const addBOnFlush = (chunks) => (chunks === null ?
    [enc.encode('-B')] : chunks);

  const data = await text(pull(from('x'), addAOnFlush, addBOnFlush));
  assert.strictEqual(data, 'x-A-B');
}

// Stateless transform flush error propagates
async function testPullStatelessTransformFlushError() {
  const badFlush = (chunks) => {
    if (chunks === null) {
      throw new Error('async flush boom');
    }
    return chunks;
  };
  await assert.rejects(async () => {
    await dump(pull(from('hello'), badFlush));
  }, { message: 'async flush boom' });
}

// An abort during an async flush must not be swallowed when the flush resolves
// to null and therefore produces no final batch.
async function testPullSignalAbortDuringAsyncFlush() {
  const ac = new AbortController();
  const reason = new Error('aborted during flush');
  const transform = async (chunks) => {
    if (chunks !== null) return chunks;
    ac.abort(reason);
    return null;
  };

  await assert.rejects(
    () => text(pull(from('x'), transform, { signal: ac.signal })),
    (error) => error === reason,
  );
}

// Pull with a sync iterable source (not async)
async function testPullWithSyncSource() {
  function* gen() {
    yield new TextEncoder().encode('sync-source');
  }
  const data = await text(pull(gen()));
  assert.strictEqual(data, 'sync-source');
}

// Pull transform yielding strings
async function testPullTransformYieldsStrings() {
  const stringTransform = (chunks) => {
    if (chunks === null) return null;
    return chunks.map((c) => new TextDecoder().decode(c));
  };
  const result = await text(pull(from('hello'), stringTransform));
  assert.strictEqual(result, 'hello');
}

// pull() accepts a string source directly (normalized via from())
async function testPullStringSource() {
  const data = await text(pull('hello-direct'));
  assert.strictEqual(data, 'hello-direct');
}

// Transform returning a single Uint8Array should be wrapped as a batch,
// not iterated byte-by-byte
async function testTransformReturnsSingleUint8Array() {
  const transform = (chunks) => {
    if (chunks === null) return null;
    // Return a single Uint8Array, not an array
    const enc = new TextEncoder();
    return enc.encode('transformed');
  };
  const data = await text(pull(from('input'), transform));
  assert.strictEqual(data, 'transformed');
}

// Transform returning a single string should be UTF-8 encoded,
// not iterated character-by-character
async function testTransformReturnsSingleString() {
  const transform = (chunks) => {
    if (chunks === null) return null;
    return 'hello-string';
  };
  const data = await text(pull(from('input'), transform));
  assert.strictEqual(data, 'hello-string');
}

// Transform returning an ArrayBuffer should be converted to Uint8Array
async function testTransformReturnsArrayBuffer() {
  const transform = (chunks) => {
    if (chunks === null) return null;
    const enc = new TextEncoder();
    return enc.encode('arraybuf').buffer;
  };
  const data = await text(pull(from('input'), transform));
  assert.strictEqual(data, 'arraybuf');
}

// pipeTo() accepts a string source directly (normalized via from())
async function testPipeToStringSource() {
  const { pipeTo, push: pushFn, text: textFn } = require('stream/iter');
  const { writer, readable } = pushFn({ budget: 16384 });
  const consume = (async () => textFn(readable))();
  await pipeTo('hello-pipe', writer);
  const data = await consume;
  assert.strictEqual(data, 'hello-pipe');
}

// Each transform receives its own options object.
// A transform that mutates options must not affect subsequent transforms.
async function testTransformOptionsNotShared() {
  const seen = [];
  const transform1 = (chunks, options) => {
    // Mutate the options object
    options.mutated = true;
    seen.push({ id: 1, mutated: options.mutated });
    return chunks;
  };
  const transform2 = (chunks, options) => {
    // Should NOT see mutation from transform1
    seen.push({ id: 2, mutated: options.mutated });
    return chunks;
  };
  await text(pull(from('test'), transform1, transform2));
  // transform1 sees its own mutation
  assert.strictEqual(seen[0].mutated, true);
  // transform2 gets a fresh options object - no mutation visible
  assert.strictEqual(seen[1].mutated, undefined);
}

// Each transform of a pipeline gets its own options object, passed to every
// call of a stateless transform. The options object has only `signal`, does
// not inherit from Object.prototype, and its prototype is frozen, so that a
// transform cannot pass state to others through it. The signal is the same
// for every transform and every call.
async function testTransformOptionsShape() {
  const seen = [];
  const statelessSeen = [];
  let statefulOptions;
  const stateless = (chunks, options) => {
    seen.push(options);
    statelessSeen.push(options);
    return chunks;
  };
  const stateful = {
    async* transform(source, options) {
      seen.push(options);
      statefulOptions = options;
      for await (const chunks of source) yield chunks;
    },
  };
  const ac = new AbortController();
  await text(pull(from(['a', 'b']), stateless, stateful,
                  { signal: ac.signal }));
  // Stateless: one call per batch plus the flush call; stateful: one call.
  assert.strictEqual(statelessSeen.length, 3);
  assert.strictEqual(new Set(statelessSeen).size, 1);
  assert.strictEqual(seen.length, 4);
  assert.notStrictEqual(statefulOptions, statelessSeen[0]);
  for (const options of seen) {
    assert.strictEqual(options instanceof Object, false);
    assert.deepStrictEqual(Object.keys(options), ['signal']);
    assert.ok(options.signal instanceof AbortSignal);
    assert.strictEqual(options.signal, seen[0].signal);
    assert.strictEqual(Object.isFrozen(Object.getPrototypeOf(options)), true);
    assert.match(inspect(options), /^TransformOptions \{ signal: /);
  }
  assert.strictEqual(Object.getPrototypeOf(statefulOptions),
                     Object.getPrototypeOf(statelessSeen[0]));
  assert.throws(() => { Object.getPrototypeOf(seen[0]).leak = true; },
                TypeError);
}

// `options.signal` can be assigned, as a data property could, whether or
// not it has been read.
async function testTransformOptionsSignalAssignable() {
  const seen = [];
  let calls = 0;
  const transform = (chunks, options) => {
    seen.push(options.signal);
    options.signal = ++calls;
    return chunks;
  };
  const unread = (chunks, options) => {
    options.signal = 'unread';
    seen.push(options.signal);
    return chunks;
  };
  await text(pull(from(['a', 'b']), transform, unread));
  assert.ok(seen[0] instanceof AbortSignal);
  assert.deepStrictEqual(seen.slice(1), ['unread', 1, 'unread', 2, 'unread']);
}

// The transforms' signal is the pipeline's: first read after the pipeline
// has been aborted, it is already aborted with the same reason, and read
// before, it is aborted when the pipeline is.
async function testTransformSignalReadAfterAbort() {
  const reason = new Error('stop');
  let options;
  const transform = (chunks, opts) => {
    options = opts;
    throw reason;
  };
  await assert.rejects(text(pull(from('a'), transform)), reason);
  assert.strictEqual(options.signal.aborted, true);
  assert.strictEqual(options.signal.reason, reason);
  assert.strictEqual(options.signal, options.signal);

  const ac = new AbortController();
  let signal;
  const iterator = pull(from(['a', 'b']), (chunks, opts) => {
    signal ??= opts.signal;
    return chunks;
  }, { signal: ac.signal })[Symbol.asyncIterator]();
  await iterator.next();
  assert.strictEqual(signal.aborted, false);
  ac.abort(reason);
  assert.strictEqual(signal.aborted, true);
  assert.strictEqual(signal.reason, reason);
}

// Run the uncaughtException test sequentially (it installs a global handler
// that would interfere with concurrent tests).
// Transform pipelines read and close the source like an async generator
// looping over it with for await. The tests below check the parts of that
// behavior that are observable from the source and the transforms' signal.

// Creates an async iterable source of batches of one-byte chunks with values
// `values`, recording calls in `log`.
function createLoggedSource(log, values, { failAt = -1 } = {}) {
  let i = 0;
  return {
    [Symbol.asyncIterator]() {
      return {
        async next() {
          log.push(`next ${i}`);
          if (i === failAt) {
            i++;
            throw new Error('source failed');
          }
          if (i >= values.length) return { done: true, value: undefined };
          return { done: false, value: [Uint8Array.of(values[i++])] };
        },
        async return() {
          log.push('return');
          return { done: true, value: undefined };
        },
      };
    },
  };
}

// A transform recording its calls, and the abort of its signal, in `log`.
function createLoggedTransform(log, transform = (chunks) => chunks) {
  let listening = false;
  return (chunks, options) => {
    log.push(chunks === null ? 'flush' : `transform ${chunks[0][0]}`);
    if (!listening) {
      listening = true;
      options.signal.addEventListener('abort', () => {
        log.push(`abort: ${options.signal.reason.message}`);
      });
    }
    return transform(chunks, options);
  };
}

async function testTransformErrorClosesSource() {
  const fail = (chunks) => {
    if (chunks?.[0][0] === 2) throw new Error('transform failed');
    return chunks;
  };
  for (const transform of [fail, async (chunks) => fail(chunks)]) {
    const log = [];
    await assert.rejects(async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of pull(createLoggedSource(log, [1, 2, 3]),
                                 createLoggedTransform(log, transform)));
    }, /transform failed/);
    assert.deepStrictEqual(log, [
      'next 0', 'transform 1', 'next 1', 'transform 2', 'return',
      'abort: transform failed',
    ]);
  }
}

async function testTransformSourceErrorDoesNotCloseSource() {
  const log = [];
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of pull(createLoggedSource(log, [1], { failAt: 1 }),
                               createLoggedTransform(log)));
  }, /source failed/);
  assert.deepStrictEqual(log, [
    'next 0', 'transform 1', 'next 1', 'abort: source failed',
  ]);
}

async function testPipeToTransformsStoppedEarly() {
  // When the writer fails, the transforms' signal is aborted, and the
  // transforms are closed, closing the source, as when the consumer of pull()
  // stops early.
  const log = [];
  let writes = 0;
  await assert.rejects(pipeTo(
    createLoggedSource(log, [1, 2, 3]), createLoggedTransform(log), {
      write() {
        if (++writes === 2) throw new Error('write failed');
      },
    }), /write failed/);
  assert.deepStrictEqual(log, [
    'next 0', 'transform 1', 'next 1', 'transform 2', 'abort: Aborted',
    'return',
  ]);
}

async function testTransformReturnClosesOutputAndSource() {
  // A transform output that is iterated asynchronously is closed, and then
  // the source, when the pipeline is stopped while reading it.
  const log = [];
  const iterator = pull(createLoggedSource(log, [1, 2]),
                        createLoggedTransform(log, async function*() {
                          try {
                            yield Uint8Array.of(1);
                            yield Uint8Array.of(2);
                          } finally {
                            log.push('output closed');
                          }
                        }))[Symbol.asyncIterator]();
  assert.strictEqual((await iterator.next()).done, false);
  assert.strictEqual((await iterator.return()).done, true);
  assert.strictEqual((await iterator.next()).done, true);
  assert.deepStrictEqual(log, [
    'next 0', 'transform 1', 'output closed', 'abort: Aborted', 'return',
  ]);
}

(async () => {
  await Promise.all([
    testPullIdentity(),
    testPullStatelessTransform(),
    testPullStatefulTransform(),
    testPullStatefulTransformReceiver(),
    testPullWithAbortSignal(),
    testPullKeepsRejectingAfterAbort(),
    testPullNormalizesSourceAtCallTime(),
    testPullPreAbortOrdering(),
    testPullChainedTransforms(),
    testPullSourceError(),
    testTapCallbackError(),
    testPullSignalAbortMidIteration(),
    testPullSignalAbortWhileSourceNextPending(),
    testPullReturnWhileSourceNextPending(),
    testTransformedConsumerReturnBeforeNext(),
    testPullSignalAbortWithTransformWhileSourceNextPending(),
    testPullConsumerBreakCleanup(),
    testPullTransformReturnsPromise(),
    testPullTransformYieldsStrings(),
    testPullStatelessTransformError(),
    testPullStatefulTransformError(),
    testPullStatelessTransformFlush(),
    testPullConsecutiveStatelessTransformFlush(),
    testPullStatelessTransformFlushError(),
    testPullSignalAbortDuringAsyncFlush(),
    testPullWithSyncSource(),
    testPullStringSource(),
    testTransformReturnsSingleUint8Array(),
    testTransformReturnsSingleString(),
    testTransformReturnsArrayBuffer(),
    testPipeToStringSource(),
    testTransformOptionsNotShared(),
    testTransformOptionsShape(),
    testTransformSignalReadAfterAbort(),
    testPullSignalAbortWhileTransformPending(),
    testPullSignalAbortWhileIdleClosesSource(),
    testPullSignalListenerRemoved(),
    testTransformOptionsSignalAssignable(),
    testTransformErrorClosesSource(),
    testTransformSourceErrorDoesNotCloseSource(),
    testPipeToTransformsStoppedEarly(),
    testTransformReturnClosesOutputAndSource(),
  ]);
  // Run after all concurrent tests complete to avoid global handler races
  await testTransformSignalListenerErrorOnSourceError();
})().then(common.mustCall());
