// Flags: --experimental-stream-iter
'use strict';

// An abort while a from() source's next() is pending: the pull, pipe or
// consumer rejects at once with the abort reason, and the source is closed
// at once, before its pending next() settles. When that next() settles
// later, nothing more happens: no write, no batch delivered, and no
// unhandled rejection (which would fail the test).

const common = require('../common');
const assert = require('assert');
const { bytes, pipeTo, pull, from } = require('stream/iter');

const tick = () => new Promise(setImmediate);

// An async source whose first next() gives one batch and whose later
// next() calls stay pending until release().
function pendingSource() {
  const log = [];
  let reads = 0;
  let release = null;
  const source = {
    [Symbol.asyncIterator]() { return this; },
    next() {
      reads++;
      log.push('next');
      if (reads === 1) {
        return Promise.resolve({ done: false, value: [new Uint8Array([1])] });
      }
      return new Promise((resolve) => {
        release = () => resolve({ done: false, value: [new Uint8Array([2])] });
      });
    },
    return() {
      log.push('return');
      return Promise.resolve({ done: true, value: undefined });
    },
  };
  return { source, log, release: () => release() };
}

async function checkPendingRead(start, { stateful = false } = {}) {
  const { source, log, release } = pendingSource();
  const ac = new AbortController();
  const reason = new Error('stop');
  const { done, readFirst, extra } = start(from(source), ac.signal, log);
  await readFirst;
  await tick();
  assert.deepStrictEqual(log, ['next', 'next']);
  ac.abort(reason);
  await assert.rejects(done, reason);
  await tick();
  // Closed while its next() is pending.
  assert.deepStrictEqual(log.slice(0, 3), ['next', 'next', 'return']);
  if (stateful) assert.ok(log.includes('finally'));
  release();
  await tick();
  await tick();
  assert.deepStrictEqual(extra(), []);
  assert.strictEqual(log.filter((x) => x === 'return').length, 1);
  assert.strictEqual(log.filter((x) => x === 'next').length, 2);
}

// pull() with a signal, with and without a stateless transform.
async function testPull(transforms) {
  await checkPendingRead((normalized, signal) => {
    const it = pull(normalized, ...transforms, { signal })[Symbol.asyncIterator]();
    const late = [];
    const readFirst = it.next();
    const done = readFirst.then(() => it.next()).then((r) => late.push(r));
    return { done, readFirst, extra: () => late };
  });
}

// pull() without a signal, stopped with return() while a pull is pending.
async function testPullReturn() {
  const { source, log, release } = pendingSource();
  const it = pull(from(source), (c) => c)[Symbol.asyncIterator]();
  await it.next();
  const pending = it.next();
  await tick();
  const returned = it.return();
  await assert.rejects(pending, { name: 'AbortError' });
  await returned;
  assert.deepStrictEqual(log, ['next', 'next', 'return']);
  release();
  await tick();
  await tick();
  assert.deepStrictEqual(log, ['next', 'next', 'return']);
}

// The same, stopped with throw().
async function testPullThrow() {
  const { source, log, release } = pendingSource();
  const it = pull(from(source), (c) => c)[Symbol.asyncIterator]();
  await it.next();
  const pending = it.next();
  await tick();
  const error = new Error('thrown');
  const thrown = it.throw(error);
  await assert.rejects(pending, error);
  await assert.rejects(thrown, error);
  assert.deepStrictEqual(log, ['next', 'next', 'return']);
  release();
  await tick();
  await tick();
  assert.deepStrictEqual(log, ['next', 'next', 'return']);
}

// A stateful transform's generator waits for the source in user code: it
// still sees the read fail at once, and its finally block runs before the
// source's next() settles.
async function testPullStateful() {
  await checkPendingRead((normalized, signal, log) => {
    const transform = {
      async *transform(source) {
        try {
          for await (const batch of source) yield batch;
        } finally {
          log.push('finally');
        }
      },
    };
    const it = pull(normalized, transform, { signal })[Symbol.asyncIterator]();
    const late = [];
    const readFirst = it.next();
    const done = readFirst.then(() => it.next()).then((r) => late.push(r));
    return { done, readFirst, extra: () => late };
  }, { stateful: true });
}

async function testPipeTo(transforms) {
  await checkPendingRead((normalized, signal) => {
    const written = [];
    let first;
    const readFirst = new Promise((resolve) => { first = resolve; });
    const writer = {
      write(chunk) { written.push(chunk[0]); first(); },
      end() {},
      fail() {},
    };
    const done = pipeTo(normalized, ...transforms, writer, { signal });
    return { done, readFirst, extra: () => written.slice(1) };
  });
}

async function testBytes() {
  await checkPendingRead((normalized, signal) => {
    // bytes() gives nothing back before the end: the first read is done
    // once the source has been read twice.
    const done = bytes(normalized, { signal });
    return { done, readFirst: tick(), extra: () => [] };
  });
}

(async () => {
  await testPull([]);
  await testPull([(c) => c]);
  await testPullReturn();
  await testPullThrow();
  await testPullStateful();
  await testPipeTo([]);
  await testPipeTo([(c) => c]);
  await testBytes();
})().then(common.mustCall());
