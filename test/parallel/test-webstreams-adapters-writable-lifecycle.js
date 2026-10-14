'use strict';

const common = require('../common');
const assert = require('assert');
const { createHook } = require('async_hooks');
const { Writable } = require('stream');

const mode = process.argv[2];
const hook = mode === 'hooks' ? createHook({ init() {} }).enable() : undefined;

async function checkBufferedWritev() {
  const consumed = [];
  let finishInitial;
  let finishBuffered;
  const writable = new Writable({
    highWaterMark: 1,
    write: common.mustCall((chunk, encoding, callback) => {
      consumed.push(Buffer.from(chunk));
      finishInitial = callback;
    }),
    writev: common.mustCall((chunks, callback) => {
      consumed.push(...chunks.map(({ chunk }) => Buffer.from(chunk)));
      finishBuffered = callback;
    }),
  });
  writable.write(Buffer.from('initial'));
  writable.write(Buffer.from('external'));
  const writer = Writable.toWeb(writable).getWriter();
  const input = Buffer.from('web');
  let fulfilled = false;
  const pending = writer.write(input).then(common.mustCall(() => { fulfilled = true; }));
  await new Promise(setImmediate);
  assert.strictEqual(fulfilled, false);
  finishInitial();
  assert.strictEqual(typeof finishBuffered, 'function');
  await new Promise(setImmediate);
  assert.strictEqual(fulfilled, false);
  finishBuffered();
  await pending;
  input.fill(255);
  await writer.close();
  assert.deepStrictEqual(consumed, [
    Buffer.from('initial'), Buffer.from('external'), Buffer.from('web'),
  ]);
}

async function checkPendingAbort(reason, highWaterMark) {
  const started = Promise.withResolvers();
  let finishWrite;
  const writable = new Writable({
    highWaterMark,
    write: common.mustCall((chunk, encoding, callback) => {
      finishWrite = callback;
      started.resolve();
    }),
  });
  const writer = Writable.toWeb(writable).getWriter();
  const pending = assert.rejects(writer.write(Buffer.from('x')), (actual) => actual === reason);
  const closed = assert.rejects(writer.closed, (actual) => actual === reason);
  await started.promise;
  await Promise.all([pending, closed, writer.abort(reason)]);
  assert.strictEqual(writable.destroyed, true);
  // A callback arriving after abort must not complete another Web write.
  finishWrite();
  await new Promise(setImmediate);
}

async function checkAbortDuringDrain() {
  const started = Promise.withResolvers();
  const error = new Error('abort during drain');
  let finishWrite;
  let abort;
  const writable = new Writable({
    highWaterMark: 1,
    write: common.mustCall((chunk, encoding, callback) => {
      finishWrite = callback;
      started.resolve();
    }),
  });
  const writer = Writable.toWeb(writable).getWriter();
  writable.on('drain', common.mustCall(() => { abort = writer.abort(error); }));
  const pending = assert.rejects(writer.write(Buffer.from('x')), (actual) => actual === error);
  const closed = assert.rejects(writer.closed, (actual) => actual === error);
  await started.promise;
  finishWrite();
  await Promise.all([abort, pending, closed]);
}

async function checkReentrantWrite() {
  const consumed = [];
  const writable = new Writable({
    write: common.mustCall(function(chunk, encoding, callback) {
      const consume = () => {
        consumed.push(chunk[0]);
        callback();
      };
      if (chunk[0] === 2) {
        setImmediate(consume);
      } else {
        consume();
        if (chunk[0] === 1) this.write(Buffer.from([2]), common.mustCall());
      }
    }, 3),
  });
  const writer = Writable.toWeb(writable).getWriter();
  const input = Buffer.from([1]);
  await writer.write(input);
  input[0] = 3;
  await writer.write(input);
  input[0] = 9;
  await writer.close();
  assert.deepStrictEqual(consumed, [1, 2, 3]);
}

async function checkConstruction(fail, synchronous = false) {
  const error = fail ? new Error('construction failed') : undefined;
  let finishConstruct;
  let consumed = false;
  const writable = new Writable({
    construct: common.mustCall((callback) => { finishConstruct = callback; }),
    write: fail ? common.mustNotCall() : common.mustCall((chunk, encoding, callback) => {
      consumed = true;
      assert.deepStrictEqual(chunk, Buffer.from('x'));
      if (synchronous) callback();
      else setImmediate(callback);
    }),
  });
  const writer = Writable.toWeb(writable).getWriter();
  const write = writer.write(Buffer.from('x'));
  const pending = fail ? assert.rejects(write, (actual) => actual === error) : write;
  const closed = fail ? assert.rejects(writer.closed, (actual) => actual === error) : undefined;
  await new Promise(setImmediate);
  assert.strictEqual(consumed, false);
  finishConstruct(error);
  await pending;
  assert.strictEqual(consumed, !fail);
  if (fail) {
    await closed;
  } else {
    await writer.close();
  }
}

async function checkPrematureClose(error, highWaterMark) {
  const started = Promise.withResolvers();
  let finishWrite;
  const writable = new Writable({
    highWaterMark,
    write: common.mustCall((chunk, encoding, callback) => {
      finishWrite = callback;
      started.resolve();
    }),
    final: common.mustNotCall(),
  });
  const expected = error === undefined ? { code: 'ABORT_ERR' } : (actual) => actual === error;
  const writer = Writable.toWeb(writable).getWriter();
  const pending = assert.rejects(writer.write(Buffer.from('x')), expected);
  const closed = assert.rejects(writer.closed, expected);
  await started.promise;
  writable.destroy(error);
  const close = assert.rejects(writer.close(), expected);
  await Promise.all([pending, closed, close]);
  finishWrite();
  await new Promise(setImmediate);
}

async function main() {
  // node:test enables async hooks and would hide the default parked path.
  await checkBufferedWritev();
  await checkAbortDuringDrain();
  await checkReentrantWrite();
  for (const highWaterMark of [1, 65536]) {
    for (const reason of [null, false, 0, '', new Error('abort')]) {
      await checkPendingAbort(reason, highWaterMark);
    }
    for (const error of [undefined, new Error('premature native close')]) {
      await checkPrematureClose(error, highWaterMark);
    }
  }
  for (const fail of [false, true]) await checkConstruction(fail);
  await checkConstruction(false, true);
  hook?.disable();

  if (mode === undefined) {
    const { code, signal, stderr } = await common.spawnPromisified(process.execPath, [__filename, 'hooks']);
    assert.strictEqual(code, 0, stderr);
    assert.strictEqual(signal, null);
  }
}

main().then(common.mustCall());
