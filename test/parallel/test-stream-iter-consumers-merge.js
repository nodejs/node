// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  dump,
  from,
  fromSync,
  merge,
  push,
  text,
  toAsyncStreamable,
  toStreamable,
} = require('stream/iter');
const { setTimeout, setImmediate } = require('timers/promises');

// =============================================================================
// merge
// =============================================================================

async function testMergeTwoSources() {
  const { writer: w1, readable: r1 } = push();
  const { writer: w2, readable: r2 } = push();

  w1.write('from-a');
  w1.end();
  w2.write('from-b');
  w2.end();

  const merged = merge(r1, r2);
  const chunks = [];
  for await (const batch of merged) {
    for (const chunk of batch) {
      chunks.push(new TextDecoder().decode(chunk));
    }
  }

  // Both sources should be present (order is temporal, not guaranteed)
  assert.strictEqual(chunks.length, 2);
  assert.ok(chunks.includes('from-a'));
  assert.ok(chunks.includes('from-b'));
}

async function testMergeSingleSource() {
  const data = await text(merge(from('only-one')));
  assert.strictEqual(data, 'only-one');
}

async function testMergeEmpty() {
  const merged = merge();
  const batches = [];
  for await (const batch of merged) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testMergeWithAbortSignal() {
  const merged = merge(from('data'), { signal: AbortSignal.abort() });

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of merged) {
        assert.fail('Should not reach here');
      }
    },
    { name: 'AbortError' },
  );
}

// Regression test: merge() with sync iterable sources
async function testMergeSyncSources() {
  const s1 = fromSync('abc');
  const s2 = fromSync('def');
  const result = await text(merge(s1, s2));
  // Both sources should be fully consumed; order may vary
  assert.strictEqual(result.length, 6);
  for (const ch of 'abcdef') {
    assert.ok(result.includes(ch), `missing '${ch}' in '${result}'`);
  }
}

// =============================================================================
// Merge error propagation
// =============================================================================

async function testMergeSourceError() {
  async function* goodSource() {
    const enc = new TextEncoder();
    yield [enc.encode('a')];
    // Slow so the bad source errors first
    await setTimeout(50);
    yield [enc.encode('b')];
  }

  async function* badSource() {
    yield [new TextEncoder().encode('x')];
    throw new Error('merge source boom');
  }
  await assert.rejects(
    async () => {
      await dump(merge(goodSource(), badSource()));
    },
    { message: 'merge source boom' },
  );
}

async function testMergeFalsySourceErrors() {
  const reasons = [undefined, null, false, 0, '', NaN];

  for (const reason of reasons) {
    const noError = { __proto__: null };
    let actual = noError;
    try {
      await text(merge(rejectedSource(reason), from('other')));
    } catch (error) {
      actual = error;
    }
    assert.strictEqual(Object.is(actual, reason), true);
  }
}

function rejectedSource(reason) {
  return {
    __proto__: null,
    [Symbol.asyncIterator]() {
      return this;
    },
    next() {
      return Promise.reject(reason);
    },
  };
}

function pendingSource() {
  return {
    __proto__: null,
    [Symbol.asyncIterator]() {
      return this;
    },
    next() {
      return new Promise(() => {});
    },
    return() {
      return new Promise(() => {});
    },
  };
}

async function testMergeSourceErrorDoesNotAwaitCleanup() {
  const reason = new Error('source failed');

  const timedOut = { __proto__: null };
  const outcome = await Promise.race([
    text(merge(rejectedSource(reason), pendingSource())).then(
      () => ({ __proto__: null, status: 'fulfilled' }),
      (error) => ({ __proto__: null, status: 'rejected', error }),
    ),
    setImmediate(timedOut),
  ]);

  assert.notStrictEqual(outcome, timedOut);
  assert.strictEqual(outcome.status, 'rejected');
  assert.strictEqual(outcome.error, reason);
}

async function testMergeBreakDoesNotAwaitCleanup() {
  async function* readySource() {
    yield [Uint8Array.of(1)];
  }

  const timedOut = { __proto__: null };
  const outcome = await Promise.race([
    (async () => {
      for await (const batch of merge(readySource(), pendingSource())) {
        assert.deepStrictEqual(batch, [Uint8Array.of(1)]);
        break;
      }
      return true;
    })(),
    setImmediate(timedOut),
  ]);

  assert.strictEqual(outcome, true);
}

async function testMergeNaNAbortDoesNotAwaitCleanup() {
  const ac = new AbortController();
  const iterator = merge(pendingSource(), pendingSource(), {
    __proto__: null,
    signal: ac.signal,
  })[Symbol.asyncIterator]();
  const next = iterator.next();
  await setImmediate();
  ac.abort(NaN);

  const timedOut = { __proto__: null };
  const outcome = await Promise.race([
    next.then(
      () => ({ __proto__: null, status: 'fulfilled' }),
      (error) => ({ __proto__: null, status: 'rejected', error }),
    ),
    setImmediate(timedOut),
  ]);

  assert.notStrictEqual(outcome, timedOut);
  assert.strictEqual(outcome.status, 'rejected');
  assert.strictEqual(Object.is(outcome.error, NaN), true);
}

async function testMergeConsumerBreak() {
  let source1Return = false;
  let source2Return = false;
  async function* source1() {
    try {
      while (true) yield [new TextEncoder().encode('a')];
    } finally {
      source1Return = true;
    }
  }

  async function* source2() {
    try {
      while (true) yield [new TextEncoder().encode('b')];
    } finally {
      source2Return = true;
    }
  }
  // eslint-disable-next-line no-unused-vars
  for await (const _ of merge(source1(), source2())) {
    break; // Break after first batch
  }
  // Give async cleanup a tick to complete
  await setImmediate();
  // Both sources should be cleaned up
  assert.strictEqual(source1Return && source2Return, true);
}

async function testMergeSignalMidIteration() {
  const ac = new AbortController();
  async function* slowSource() {
    const enc = new TextEncoder();
    yield [enc.encode('a')];
    await setTimeout(100);
    yield [enc.encode('b')];
  }
  const merged = merge(slowSource(), { signal: ac.signal });
  const iter = merged[Symbol.asyncIterator]();
  await iter.next(); // First batch
  ac.abort();
  await assert.rejects(() => iter.next(), { name: 'AbortError' });
}

async function testMergeSignalDuringPendingMultiSourceRead() {
  const ac = new AbortController();

  async function* pending() {
    await new Promise(() => {});
    yield [];
  }

  const iter = merge(pending(), pending(), {
    __proto__: null,
    signal: ac.signal,
  })[Symbol.asyncIterator]();

  const next = iter.next();
  ac.abort();

  await assert.rejects(next, { name: 'AbortError' });
}

async function testMergeSignalDuringPendingSingleSourceRead() {
  const ac = new AbortController();
  let returned = false;
  const source = {
    __proto__: null,
    [Symbol.asyncIterator]() {
      return this;
    },
    next() {
      // Intentionally never settle to verify that aborting interrupts a pending read.
      return new Promise(() => {});
    },
    return() {
      returned = true;
      return { __proto__: null, done: true };
    },
  };

  const iter = merge(source, {
    __proto__: null,
    signal: ac.signal,
  })[Symbol.asyncIterator]();

  const next = iter.next();
  await setImmediate();
  ac.abort();

  await assert.rejects(next, { name: 'AbortError' });
  assert.strictEqual(returned, true);
}

async function testMergeDoesNotDrainSourcesWhileIdle() {
  function source(n) {
    return {
      __proto__: null,
      pulls: 0,
      async *[Symbol.asyncIterator]() {
        while (this.pulls < n) {
          yield [Buffer.from(`${++this.pulls}`)];
        }
      },
    };
  }

  const a = source(5);
  const b = source(5);
  const iterator = merge(a, b)[Symbol.asyncIterator]();

  await iterator.next();
  await setImmediate();

  assert.strictEqual(a.pulls, 1);
  assert.strictEqual(b.pulls, 1);

  await iterator.return?.();
}

// merge() accepts string sources (normalized via from())
async function testMergeStringSources() {
  const batches = [];
  for await (const batch of merge('hello', 'world')) {
    batches.push(batch);
  }
  // Each string becomes a single-batch source
  assert.strictEqual(batches.length >= 2, true);
  const combined = new TextDecoder().decode(Buffer.concat(batches.flat()));
  // Both strings should appear (order may vary)
  assert.ok(combined.includes('hello'));
  assert.ok(combined.includes('world'));
}

// merge() accepts object-like sources that are normalized via from()
async function testMergeObjectLikeSources() {
  const arrayBuffer = new TextEncoder().encode('abc').buffer;
  const dataView = new DataView(new TextEncoder().encode('def').buffer);
  const streamable = {
    [toStreamable]() {
      return 'ghi';
    },
  };
  const asyncStreamable = {
    [toAsyncStreamable]() {
      return Promise.resolve('jkl');
    },
  };

  assert.strictEqual(await text(merge(arrayBuffer)), 'abc');
  assert.strictEqual(await text(merge(dataView)), 'def');
  assert.strictEqual(await text(merge(streamable)), 'ghi');
  assert.strictEqual(await text(merge(asyncStreamable)), 'jkl');
}

// =============================================================================
// Merge cleanup error handling
// =============================================================================

function throwInFinally(message) {
  throw new Error(message);
}

// Cleanup error with no primary error: iterator.return() throws during
// normal completion. The cleanup error should propagate directly.
async function testMergeCleanupErrorOnly() {
  async function* source() {
    yield [new TextEncoder().encode('data')];
  }

  async function* failingReturnSource() {
    try {
      yield [new TextEncoder().encode('more')];
    } finally {
      throwInFinally('cleanup boom');
    }
  }

  await assert.rejects(
    async () => {
      await dump(merge(source(), failingReturnSource()));
    },
    { message: 'cleanup boom' },
  );
}

// A primary source error must not wait for asynchronous cleanup failures.
async function testMergePrimaryErrorPrecedesCleanupError() {
  async function* badSource() {
    yield [new TextEncoder().encode('x')];
    throw new Error('primary boom');
  }

  async function* failingReturnSource() {
    try {
      while (true) yield [new TextEncoder().encode('y')];
    } finally {
      throwInFinally('cleanup boom');
    }
  }

  await assert.rejects(
    async () => {
      await dump(merge(badSource(), failingReturnSource()));
    },
    { message: 'primary boom' },
  );
}

// Consumer break + cleanup error: consumer breaks and iterator.return()
// throws. The cleanup error should propagate.
async function testMergeBreakWithCleanupError() {
  async function* failingReturnSource() {
    try {
      while (true) yield [new TextEncoder().encode('data')];
    } finally {
      throwInFinally('cleanup on break');
    }
  }

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of merge(failingReturnSource())) {
        break;
      }
    },
    { message: 'cleanup on break' },
  );
}

async function testMergeMultiSourceBreakWithCleanupError() {
  async function* failingReturnSource() {
    try {
      yield [new TextEncoder().encode('data')];
    } finally {
      await Promise.resolve();
      throwInFinally('async cleanup on break');
    }
  }

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of merge(failingReturnSource(), from('other'))) {
        break;
      }
    },
    { message: 'async cleanup on break' },
  );
}

// merge() of a single source behaves as an async generator reading it:
// an abort rejects the pending read, closes the source and ends the
// iteration; an abort before the first read does not open the source;
// return() is queued behind pending reads.
async function testMergeSingleSourceProtocol() {
  const log = [];
  function source(name, values, { hang = false } = {}) {
    let i = 0;
    return {
      [Symbol.asyncIterator]() {
        log.push(`${name} iterator`);
        return {
          next() {
            log.push(`${name} next ${i}`);
            if (hang && i === values.length) return new Promise(() => {});
            return Promise.resolve(i < values.length ?
              { done: false, value: values[i++] } : { done: true });
          },
          return() {
            log.push(`${name} return`);
            return Promise.resolve({ done: true });
          },
        };
      },
    };
  }

  async function settle(label, promise) {
    try {
      const result = await promise;
      log.push(`${label}: ${result.done ? 'done' : result.value[0][0]}`);
    } catch (error) {
      log.push(`${label}: ${error.name}`);
    }
  }
  const chunk = (n) => [Uint8Array.of(n)];

  const aborted = AbortSignal.abort();
  let it = merge(source('a', [chunk(1)]), { signal: aborted })[
    Symbol.asyncIterator]();
  await settle('a 1', it.next());
  await settle('a 2', it.next());

  const ac = new AbortController();
  it = merge(source('b', [chunk(1)], { hang: true }), { signal: ac.signal })[
    Symbol.asyncIterator]();
  await settle('b 1', it.next());
  const pending = it.next();
  await setImmediate();
  ac.abort();
  await settle('b 2', pending);
  await settle('b 3', it.next());

  it = merge(source('c', [chunk(1), 'x', chunk(3)]))[Symbol.asyncIterator]();
  const results = [it.next(), it.next(), it.return(), it.next()];
  for (let i = 0; i < results.length; i++) await settle(`c ${i}`, results[i]);

  it = merge(source('d', [chunk(1)]))[Symbol.asyncIterator]();
  await settle('d return', it.return());
  await assert.rejects(it.throw(new Error('thrown')), { message: 'thrown' });

  assert.deepStrictEqual(log, [
    'a 1: AbortError', 'a 2: done',
    'b iterator', 'b next 0', 'b 1: 1', 'b next 1', 'b return',
    'b 2: AbortError', 'b 3: done',
    'c iterator', 'c next 0', 'c next 1', 'c 0: 1', 'c 1: 120', 'c return',
    'c 2: done', 'c 3: done',
    'd return: done',
  ]);
}

Promise.all([
  testMergeSingleSourceProtocol(),
  testMergeTwoSources(),
  testMergeSingleSource(),
  testMergeEmpty(),
  testMergeWithAbortSignal(),
  testMergeSyncSources(),
  testMergeSourceError(),
  testMergeFalsySourceErrors(),
  testMergeSourceErrorDoesNotAwaitCleanup(),
  testMergeBreakDoesNotAwaitCleanup(),
  testMergeNaNAbortDoesNotAwaitCleanup(),
  testMergeConsumerBreak(),
  testMergeSignalMidIteration(),
  testMergeSignalDuringPendingMultiSourceRead(),
  testMergeSignalDuringPendingSingleSourceRead(),
  testMergeDoesNotDrainSourcesWhileIdle(),
  testMergeStringSources(),
  testMergeObjectLikeSources(),
  testMergeCleanupErrorOnly(),
  testMergePrimaryErrorPrecedesCleanupError(),
  testMergeBreakWithCleanupError(),
  testMergeMultiSourceBreakWithCleanupError(),
]).then(common.mustCall());
