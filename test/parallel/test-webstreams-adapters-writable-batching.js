'use strict';

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');

async function checkReusableBatches() {
  const consumed = [];
  let batches = 0;
  let drains = 0;
  const writable = new Writable({
    highWaterMark: 64 * 1024,
    write(chunk, encoding, callback) {
      setImmediate(() => {
        consumed.push(Buffer.from(chunk));
        callback();
      });
    },
    writev(chunks, callback) {
      batches++;
      setImmediate(() => {
        consumed.push(...chunks.map(({ chunk }) => Buffer.from(chunk)));
        callback();
      });
    },
  });
  writable.on('error', common.mustNotCall());
  writable.on('drain', () => { drains++; });
  const writer = Writable.toWeb(writable).getWriter();
  const input = Buffer.alloc(1024);
  for (let i = 1; i <= 128; i++) {
    input.fill(i);
    await writer.write(input);
    if (i === 1) assert.strictEqual(consumed.length, 1);
    input.fill(255);
  }
  await writer.close();

  assert(batches > 0, 'small awaited writes should reach native writev');
  assert(drains > 0, 'buffered small writes should observe native backpressure');
  assert.strictEqual(consumed.length, 128);
  for (let i = 1; i <= 128; i++) {
    assert.deepStrictEqual(consumed[i - 1], Buffer.alloc(1024, i));
  }
}

async function checkLateWriteError() {
  const error = new Error('late native write failure');
  const started = Promise.withResolvers();
  let finishWrite;
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      if (chunk[0] === 1) {
        setImmediate(callback);
      } else {
        finishWrite = callback;
        started.resolve();
      }
    }, 2),
  });
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));

  const closed = assert.rejects(writer.closed, (actual) => actual === error);
  let fulfilled = false;
  const smallWrite = writer.write(Buffer.from([2])).then(() => {
    fulfilled = true;
    return true;
  }, (actual) => {
    assert.strictEqual(actual, error);
    return false;
  });
  await started.promise;
  await new Promise(setImmediate);
  const fulfilledBeforeError = fulfilled;
  const close = assert.rejects(writer.close(), (actual) => actual === error);
  finishWrite(error);
  const accepted = await smallWrite;
  await Promise.all([closed, close]);

  assert.strictEqual(fulfilledBeforeError, true);
  assert.strictEqual(accepted, true);
}

async function checkLargeWrite(size) {
  const started = Promise.withResolvers();
  const input = new Uint8Array(new ArrayBuffer(size + 8), 4, size);
  input.fill(3);
  let consumed;
  let finishWrite;
  const writable = new Writable({
    highWaterMark: 4,
    write: common.mustCall((chunk, encoding, callback) => {
      if (chunk[0] === 1) {
        setImmediate(callback);
      } else {
        assert.strictEqual(chunk.buffer, input.buffer);
        assert.strictEqual(chunk.byteOffset, input.byteOffset);
        finishWrite = () => {
          consumed = Buffer.from(chunk);
          callback();
        };
        started.resolve();
      }
    }, 2),
  });
  writable.on('error', common.mustNotCall());
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));

  let fulfilled = false;
  const pending = writer.write(input).then(() => { fulfilled = true; });
  await started.promise;
  await new Promise(setImmediate);
  const fulfilledBeforeCompletion = fulfilled;
  finishWrite();
  await pending;
  input.fill(255);
  await writer.close();

  assert.strictEqual(fulfilledBeforeCompletion, false);
  assert.deepStrictEqual(consumed, Buffer.alloc(size, 3));
}

async function checkLargeWriteAfterCopiedBatch() {
  const consumed = [];
  const batchStarted = Promise.withResolvers();
  const input = Buffer.alloc(1);
  const large = Buffer.alloc(4, 5);
  let finishInitial;
  let finishBatch;
  const writable = new Writable({
    highWaterMark: 4,
    write: common.mustCall((chunk, encoding, callback) => {
      const consume = () => {
        consumed.push(Buffer.from(chunk));
        callback();
      };
      if (chunk[0] === 1) setImmediate(consume);
      else finishInitial = consume;
    }, 2),
    writev: common.mustCall((chunks, callback) => {
      assert.strictEqual(chunks.length, 3);
      assert.strictEqual(chunks[2].chunk.buffer, large.buffer);
      finishBatch = () => {
        consumed.push(...chunks.map(({ chunk }) => Buffer.from(chunk)));
        callback();
      };
      batchStarted.resolve();
    }),
  });
  writable.on('error', common.mustNotCall());
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));
  for (const value of [2, 3, 4]) {
    input[0] = value;
    await writer.write(input);
    input[0] = 255;
  }

  let fulfilled = false;
  const pending = writer.write(large).then(() => { fulfilled = true; });
  await new Promise(setImmediate);
  assert.strictEqual(fulfilled, false);
  finishInitial();
  await batchStarted.promise;
  await new Promise(setImmediate);
  const fulfilledBeforeBatchCompletion = fulfilled;
  finishBatch();
  await pending;
  large.fill(255);
  await writer.close();

  assert.strictEqual(fulfilledBeforeBatchCompletion, false);
  assert.deepStrictEqual(consumed, [
    Buffer.from([1]), Buffer.from([2]), Buffer.from([3]),
    Buffer.from([4]), Buffer.from([5, 5, 5, 5]),
  ]);
}

async function checkQueuedWritevError() {
  const error = new Error('queued native writev failure');
  const batchStarted = Promise.withResolvers();
  let finishInitial;
  let finishBatch;
  const writable = new Writable({
    highWaterMark: 4,
    write: common.mustCall((chunk, encoding, callback) => {
      if (chunk[0] === 1) setImmediate(callback);
      else finishInitial = callback;
    }, 2),
    writev: common.mustCall((chunks, callback) => {
      assert.deepStrictEqual(chunks.map(({ chunk }) => Array.from(chunk)), [[3], [4]]);
      finishBatch = callback;
      batchStarted.resolve();
    }),
  });
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));
  for (const value of [2, 3, 4]) await writer.write(Buffer.from([value]));

  const closed = assert.rejects(writer.closed, (actual) => actual === error);
  const close = assert.rejects(writer.close(), (actual) => actual === error);
  finishInitial();
  await batchStarted.promise;
  setImmediate(finishBatch, error);
  await Promise.all([closed, close]);
}

async function checkSyncWriteErrorAfterAsyncWrite() {
  const error = new Error('synchronous native write failure');
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      if (chunk[0] === 1) setImmediate(callback);
      else callback(error);
    }, 2),
  });
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));
  await Promise.all([
    assert.rejects(writer.write(Buffer.from([2])), (actual) => actual === error),
    assert.rejects(writer.closed, (actual) => actual === error),
  ]);
}

async function main() {
  await checkReusableBatches();
  await checkLateWriteError();
  for (const size of [4, 5]) await checkLargeWrite(size);
  await checkLargeWriteAfterCopiedBatch();
  await checkQueuedWritevError();
  await checkSyncWriteErrorAfterAsyncWrite();
}

main().then(common.mustCall());
