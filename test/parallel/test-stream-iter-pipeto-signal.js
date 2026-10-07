// Flags: --experimental-stream-iter
'use strict';

// Tests for pipeTo with live (non-pre-aborted) AbortSignal,
// both with and without transforms.

const common = require('../common');
const assert = require('assert');
const { setTimeout } = require('timers/promises');
const { getEventListeners } = require('events');
const { bytes, pipeTo, pull, from } = require('stream/iter');

async function testPipeToPreAbortedSignalFailsWriter() {
  const reason = new Error('already aborted');
  let sourceTouched = false;
  const source = {
    [Symbol.asyncIterator]() {
      sourceTouched = true;
      return {};
    },
  };
  const writer = {
    write: common.mustNotCall(),
    fail: common.mustCall((error) => assert.strictEqual(error, reason)),
  };

  await assert.rejects(
    pipeTo(source, writer, { signal: AbortSignal.abort(reason) }),
    (error) => error === reason,
  );
  assert.strictEqual(sourceTouched, false);
}

async function testPipeToPreAbortedSignalPreventFail() {
  const reason = new Error('already aborted');
  let sourceTouched = false;
  const source = {
    [Symbol.asyncIterator]() {
      sourceTouched = true;
      return {};
    },
  };
  const writer = {
    write: common.mustNotCall(),
    fail: common.mustNotCall(),
  };

  await assert.rejects(
    pipeTo(source, writer, {
      signal: AbortSignal.abort(reason),
      preventFail: true,
    }),
    (error) => error === reason,
  );
  assert.strictEqual(sourceTouched, false);
}

// pipeTo with live signal, no transforms — abort mid-stream
async function testPipeToLiveSignalNoTransforms() {
  const ac = new AbortController();
  const written = [];
  const writer = {
    async write(chunk) { written.push(chunk); },
    async end() {},
  };
  async function* source() {
    yield [new Uint8Array([1])];
    yield [new Uint8Array([2])];
    ac.abort();
    yield [new Uint8Array([3])];
  }
  await assert.rejects(
    () => pipeTo(source(), writer, { signal: ac.signal }),
    { name: 'AbortError' },
  );
  // Should have written at least the first two chunks before abort
  assert.ok(written.length >= 1);
}

// pipeTo with live signal, no transforms — abort while waiting for next chunk
async function testPipeToLiveSignalNoTransformsPendingNext() {
  const ac = new AbortController();
  const reason = new Error('abort reason');
  const writer = {
    write: common.mustNotCall(),
  };
  const source = {
    [Symbol.asyncIterator]() {
      return {
        next() {
          return new Promise(() => {});
        },
      };
    },
  };

  setTimeout(10)
    .then(() => ac.abort(reason))
    .then(common.mustCall());

  const result = await Promise.race([
    assert.rejects(
      () => pipeTo(source, writer, { signal: ac.signal }),
      reason,
    ).then(() => 'aborted'),
    setTimeout(1000, 'timed out'),
  ]);

  assert.strictEqual(result, 'aborted');
}

// pipeTo with live signal + transforms — abort mid-stream
async function testPipeToLiveSignalWithTransforms() {
  const ac = new AbortController();
  const written = [];
  const writer = {
    async write(chunk) { written.push(chunk); },
    async end() {},
  };
  const identity = (chunks) => chunks;
  async function* source() {
    yield [new Uint8Array([10])];
    yield [new Uint8Array([20])];
    ac.abort();
    yield [new Uint8Array([30])];
  }
  await assert.rejects(
    () => pipeTo(source(), identity, writer, { signal: ac.signal }),
    { name: 'AbortError' },
  );
  assert.ok(written.length >= 1);
}

// pipeTo with live signal, no abort — runs to completion
async function testPipeToLiveSignalCompletes() {
  const ac = new AbortController();
  const written = [];
  const writer = {
    write(chunk) { written.push(chunk); },
    writeSync(chunk) { written.push(chunk); return true; },
    async end() {},
    endSync() { return written.length; },
  };
  await pipeTo(from('signal-ok'), writer, { signal: ac.signal });
  assert.ok(written.length > 0);
}

// pipeTo with live signal + transforms, no abort — runs to completion
async function testPipeToLiveSignalWithTransformsCompletes() {
  const ac = new AbortController();
  const written = [];
  const writer = {
    write(chunk) { written.push(chunk); },
    writeSync(chunk) { written.push(chunk); return true; },
    async end() {},
    endSync() { return written.length; },
  };
  const identity = (chunks) => chunks;
  await pipeTo(from('signal-tx-ok'), identity, writer,
               { signal: ac.signal });
  assert.ok(written.length > 0);
}

async function testSignalAbortedWhileReadingSource() {
  // The signal can abort while the source is producing a value; the read
  // must still reject with the abort reason, and the source be closed, even
  // if the value never comes.
  const identity = (chunks) => chunks;
  for (const consume of [
    (source, signal) => pipeTo(source, { write() {} }, { signal }),
    (source, signal) => pipeTo(source, identity, { write() {} }, { signal }),
    (source, signal) => bytes(source, { signal }),
    (source, signal) => bytes(pull(source, { signal })),
    (source, signal) => bytes(pull(source, identity, { signal })),
  ]) {
    const ac = new AbortController();
    const reason = new Error('aborted while reading');
    let closed = false;
    const source = {
      [Symbol.asyncIterator]() {
        return {
          next() {
            ac.abort(reason);
            return new Promise(() => {});
          },
          async return() {
            closed = true;
            return { done: true };
          },
        };
      },
    };
    await assert.rejects(consume(source, ac.signal), reason);
    assert.strictEqual(closed, true);
  }
}

// write(), writev() and end() are all passed the same options object.
async function testWriteOptionsShared() {
  const ac = new AbortController();
  const seen = [];
  const writer = {
    async write(chunk, options) { seen.push(options); },
    async writev(chunks, options) { seen.push(options); },
    async end(options) { seen.push(options); },
  };
  async function* source() {
    yield [new Uint8Array([1])];
    yield [new Uint8Array([2]), new Uint8Array([3])];
    yield [new Uint8Array([4])];
  }
  await pipeTo(source(), writer, { signal: ac.signal });
  assert.strictEqual(seen.length, 4);
  assert.strictEqual(new Set(seen).size, 1);
  assert.strictEqual(seen[0].signal, ac.signal);
}

// The signal aborting while a chunk is written synchronously stops the pipe:
// no other chunk is written, the source is closed, and the pipe rejects with
// the abort reason.
async function testAbortDuringWriteSync() {
  for (const sync of [true, false]) {
    const ac = new AbortController();
    const reason = new Error('abort in writeSync');
    const written = [];
    let closed = false;
    function* syncSource() {
      try {
        for (let i = 0; i < 5; i++) yield [new Uint8Array([i])];
      } finally {
        closed = true;
      }
    }

    async function* asyncSource() {
      try {
        for (let i = 0; i < 5; i++) yield [new Uint8Array([i])];
      } finally {
        closed = true;
      }
    }
    const writer = {
      write: common.mustNotCall(),
      writeSync(chunk) {
        written.push(chunk[0]);
        if (chunk[0] === 1) ac.abort(reason);
        return true;
      },
      fail: common.mustCall((error) => assert.strictEqual(error, reason)),
    };
    await assert.rejects(
      pipeTo(sync ? syncSource() : asyncSource(), writer,
             { signal: ac.signal }),
      reason);
    assert.deepStrictEqual(written, [0, 1]);
    await setTimeout(1);
    assert.strictEqual(closed, true);
  }
}

// The signal aborting while a write is pending rejects the pipe at once,
// even if the write never completes.
async function testAbortWhileWritePending() {
  const ac = new AbortController();
  const reason = new Error('abort while writing');
  const writer = {
    write() {
      ac.abort(reason);
      return new Promise(() => {});
    },
  };
  await assert.rejects(pipeTo(from('a'), writer, { signal: ac.signal }),
                       reason);
}

async function testSignalListenersRemoved() {
  // No abort listener is left on the signal once reading completes, fails
  // or is aborted.
  const ac = new AbortController();
  await pipeTo(from('abc'), { write() {} }, { signal: ac.signal });
  await bytes(from('abc'), { signal: ac.signal });
  await assert.rejects(bytes((async function*() {
    yield 'a';
    throw new Error('source failed');
  })(), { signal: ac.signal }), /source failed/);
  assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);

  const aborting = new AbortController();
  await assert.rejects(bytes((async function*() {
    yield 'a';
    aborting.abort();
    yield 'b';
  })(), { signal: aborting.signal }), { name: 'AbortError' });
  assert.strictEqual(getEventListeners(aborting.signal, 'abort').length, 0);
}

Promise.all([
  testPipeToPreAbortedSignalFailsWriter(),
  testPipeToPreAbortedSignalPreventFail(),
  testPipeToLiveSignalNoTransforms(),
  testPipeToLiveSignalNoTransformsPendingNext(),
  testPipeToLiveSignalWithTransforms(),
  testPipeToLiveSignalCompletes(),
  testPipeToLiveSignalWithTransformsCompletes(),
  testSignalAbortedWhileReadingSource(),
  testSignalListenersRemoved(),
  testWriteOptionsShared(),
  testAbortDuringWriteSync(),
  testAbortWhileWritePending(),
]).then(common.mustCall());
