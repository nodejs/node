// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from,
  fromSync,
  push,
  merge,
  text,
  toAsyncStreamable,
  toStreamable,
} = require('stream/iter');

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
  const ac = new AbortController();
  ac.abort();

  const merged = merge(from('data'), { signal: ac.signal });

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
    await new Promise((r) => setTimeout(r, 50));
    yield [enc.encode('b')];
  }

  async function* badSource() {
    yield [new TextEncoder().encode('x')];
    throw new Error('merge source boom');
  }
  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of merge(goodSource(), badSource())) {
        /* consume */
      }
    },
    { message: 'merge source boom' },
  );
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
  await new Promise(setImmediate);
  // Both sources should be cleaned up
  assert.strictEqual(source1Return && source2Return, true);
}

async function testMergeSignalMidIteration() {
  const ac = new AbortController();
  async function* slowSource() {
    const enc = new TextEncoder();
    yield [enc.encode('a')];
    await new Promise((r) => setTimeout(r, 100));
    yield [enc.encode('b')];
  }
  const merged = merge(slowSource(), { signal: ac.signal });
  const iter = merged[Symbol.asyncIterator]();
  await iter.next(); // First batch
  ac.abort();
  await assert.rejects(() => iter.next(), { name: 'AbortError' });
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
      // eslint-disable-next-line no-unused-vars
      for await (const _ of merge(source(), failingReturnSource())) {
        // Consume all - no primary error
      }
    },
    { message: 'cleanup boom' },
  );
}

// Primary error + cleanup error: a source throws during iteration AND
// iterator.return() also throws. Should get a SuppressedError.
async function testMergePrimaryAndCleanupError() {
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
      // eslint-disable-next-line no-unused-vars
      for await (const _ of merge(badSource(), failingReturnSource())) {
        // Consume until error
      }
    },
    (err) => {
      assert.ok(
        err instanceof SuppressedError,
        `Expected SuppressedError, got ${err.constructor.name}`,
      );
      assert.strictEqual(err.error.message, 'primary boom');
      assert.strictEqual(err.suppressed.message, 'cleanup boom');
      return true;
    },
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

Promise.all([
  testMergeTwoSources(),
  testMergeSingleSource(),
  testMergeEmpty(),
  testMergeWithAbortSignal(),
  testMergeSyncSources(),
  testMergeSourceError(),
  testMergeConsumerBreak(),
  testMergeSignalMidIteration(),
  testMergeStringSources(),
  testMergeObjectLikeSources(),
  testMergeCleanupErrorOnly(),
  testMergePrimaryAndCleanupError(),
  testMergeBreakWithCleanupError(),
]).then(common.mustCall());
