'use strict';

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');
const test = require('node:test');

test('synchronous Buffer writes on an active stream complete without another tick', async () => {
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => callback(), 2),
  });
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from('hello'));

  // Run in a Promise reaction so a nextTick cannot run until these
  // microtasks finish unless the write itself waits for that tick.
  await Promise.resolve();
  let ticked = false;
  process.nextTick(() => { ticked = true; });
  await writer.write(Buffer.from('hello'));
  assert.strictEqual(ticked, false);
  await writer.close();
});

for (const ctor of [ArrayBuffer, SharedArrayBuffer]) {
  test(`synchronous Uint8Array writes preserve the ${ctor.name} view without another tick`, async () => {
    const buffer = new ctor(8);
    const input = new Uint8Array(buffer, 2, 4);
    input.set([1, 2, 3, 4]);
    const writable = new Writable({
      highWaterMark: 1,
      write: common.mustCall((chunk, encoding, callback) => {
        assert(Buffer.isBuffer(chunk));
        assert.strictEqual(chunk.buffer, buffer);
        assert.strictEqual(chunk.byteOffset, 2);
        assert.deepStrictEqual(chunk, Buffer.from([1, 2, 3, 4]));
        callback();
      }, 2),
    });
    writable.on('drain', common.mustNotCall());
    const writer = Writable.toWeb(writable).getWriter();
    await writer.write(input);

    await Promise.resolve();
    let ticked = false;
    process.nextTick(() => { ticked = true; });
    await writer.write(input);
    assert.strictEqual(ticked, false);
    await writer.close();
  });
}

test('mixed synchronous and asynchronous writes preserve reusable bytes', async () => {
  const consumed = [];
  const writable = new Writable({
    write(chunk, encoding, callback) {
      const consume = () => {
        consumed.push(chunk[0]);
        callback();
      };
      if (chunk[0] % 2 === 0) {
        setImmediate(consume);
      } else {
        consume();
      }
    },
  });
  const writer = Writable.toWeb(writable).getWriter();
  const input = Buffer.alloc(1);
  for (let i = 1; i <= 6; i++) {
    input[0] = i;
    await writer.write(input);
    input[0] = 9;
  }
  await writer.close();
  assert.deepStrictEqual(consumed, [1, 2, 3, 4, 5, 6]);
});

test('synchronous write callback errors reject the write and closed promises', async () => {
  const error = new Error('sync callback error');
  const writable = new Writable({
    write(chunk, encoding, callback) {
      callback(chunk[0] === 1 ? undefined : error);
    },
  });
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));
  await Promise.all([
    assert.rejects(writer.write(Buffer.from([2])), (actual) => actual === error),
    assert.rejects(writer.closed, (actual) => actual === error),
  ]);
});

test('falsy native callback errors still complete the write', async () => {
  const errors = [undefined, null, false, 0, ''];
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      callback(errors[chunk[0]]);
    }, errors.length),
  });
  const writer = Writable.toWeb(writable).getWriter();
  for (let i = 0; i < errors.length; i++) {
    await writer.write(Buffer.from([i]));
  }
  await writer.close();
});

test('abort releases an asynchronous write after synchronous writes', async () => {
  let finishWrite;
  let startWrite;
  const started = new Promise((resolve) => { startWrite = resolve; });
  const error = new Error('abort after sync writes');
  const writable = new Writable({
    write(chunk, encoding, callback) {
      if (chunk[0] === 1) {
        callback();
      } else {
        finishWrite = callback;
        startWrite();
      }
    },
  });
  const writer = Writable.toWeb(writable).getWriter();
  for (let i = 0; i < 2; i++) {
    await writer.write(Buffer.from([1]));
  }
  const writePromise = writer.write(Buffer.from([2]));
  await started;
  await Promise.all([
    assert.rejects(writePromise, (actual) => actual === error),
    assert.rejects(writer.closed, (actual) => actual === error),
    writer.abort(error),
  ]);
  finishWrite();
  await new Promise(setImmediate);
  assert.strictEqual(writable.destroyed, true);
});

test('abort during a synchronous write rejects its completion', async () => {
  const error = new Error('abort during write');
  let writer;
  let abortPromise;
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      if (chunk[0] === 2) {
        abortPromise = writer.abort(error);
      }
      callback();
    }, 2),
  });
  writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));
  await Promise.all([
    assert.rejects(writer.write(Buffer.from([2])), (actual) => actual === error),
    assert.rejects(writer.closed, (actual) => actual === error),
  ]);
  await abortPromise;
  assert.strictEqual(writable.destroyed, true);
});

for (const highWaterMark of [0, 1]) {
  test(`synchronous and empty writes with highWaterMark ${highWaterMark}`, async () => {
    const consumed = [];
    const writable = new Writable({
      highWaterMark,
      write(chunk, encoding, callback) {
        consumed.push(Buffer.from(chunk));
        callback();
      },
    });
    const writer = Writable.toWeb(writable).getWriter();
    for (const input of [Buffer.from([1]), Buffer.alloc(0), Buffer.from([2])]) {
      await writer.write(input);
      input.fill(9);
      assert.strictEqual(writable.writableNeedDrain, false);
    }
    await writer.close();
    assert.deepStrictEqual(consumed, [Buffer.from([1]), Buffer.alloc(0), Buffer.from([2])]);
  });
}

test('asynchronous writes still wait for drain', async () => {
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, encoding, callback) {
      setImmediate(callback);
    },
  });
  writable.on('drain', common.mustCall(3));
  const writer = Writable.toWeb(writable).getWriter();
  for (let i = 0; i < 3; i++) {
    await writer.write(Buffer.from([1]));
    assert.strictEqual(writable.writableNeedDrain, false);
  }
  await writer.close();
});
