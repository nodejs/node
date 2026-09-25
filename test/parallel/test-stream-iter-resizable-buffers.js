// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  array,
  arraySync,
  broadcast,
  pipeTo,
  pipeToSync,
  push,
  share,
  shareSync,
} = require('stream/iter');

const kResizeError = {
  code: 'ERR_INVALID_STATE',
  message: /resized or detached/,
};

async function testBufferedViewMutationRejected() {
  const resizable = new ArrayBuffer(1, { maxByteLength: 2 });
  const growable = new SharedArrayBuffer(1, { maxByteLength: 2 });
  const detachable = new ArrayBuffer(1);
  const cases = [
    [new Uint8Array(resizable), () => resizable.resize(2)],
    [new Uint8Array(growable), () => growable.grow(2)],
    [new Uint8Array(detachable), () => {
      structuredClone(detachable, { transfer: [detachable] });
    }],
  ];

  for (const [view, mutate] of cases) {
    const { writer, readable } = push();
    assert.strictEqual(writer.writeSync(view), true);
    mutate();
    await assert.rejects(
      readable[Symbol.asyncIterator]().next(),
      kResizeError,
    );
  }
}

async function testDropOldestUsesAcceptedByteLength() {
  const buffer = new ArrayBuffer(16384, { maxByteLength: 16384 });
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'drop-oldest',
  });
  const iterator = bc.push()[Symbol.asyncIterator]();

  assert.strictEqual(writer.writeSync(new Uint8Array(buffer)), true);
  buffer.resize(0);
  assert.strictEqual(writer.writeSync(Uint8Array.of(2)), true);
  assert.strictEqual(writer.writeSync(Uint8Array.of(3)), true);
  writer.endSync();

  assert.strictEqual((await iterator.next()).value[0][0], 2);
  assert.strictEqual((await iterator.next()).value[0][0], 3);
  assert.strictEqual((await iterator.next()).done, true);
}

async function testPendingWritesRejectResizedViews() {
  const pushResult = push({ budget: 16384, backpressure: 'unbounded' });
  assert.strictEqual(
    pushResult.writer.writeSync(new Uint8Array(16384)), true);
  const pushBuffer = new ArrayBuffer(1, { maxByteLength: 2 });
  const pushPending = pushResult.writer.write(new Uint8Array(pushBuffer));
  const pushRejected = assert.rejects(pushPending, kResizeError);
  pushBuffer.resize(2);
  const pushIterator = pushResult.readable[Symbol.asyncIterator]();
  assert.strictEqual((await pushIterator.next()).done, false);
  await pushRejected;
  pushResult.writer.endSync();
  assert.strictEqual((await pushIterator.next()).done, true);

  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'unbounded',
  });
  const broadcastIterator = bc.push()[Symbol.asyncIterator]();
  assert.strictEqual(writer.writeSync(new Uint8Array(16384)), true);
  const broadcastBuffer = new ArrayBuffer(1, { maxByteLength: 2 });
  const broadcastPending = writer.write(new Uint8Array(broadcastBuffer));
  const broadcastRejected = assert.rejects(broadcastPending, kResizeError);
  broadcastBuffer.resize(2);
  assert.strictEqual((await broadcastIterator.next()).done, false);
  await broadcastRejected;
  writer.endSync();
  assert.strictEqual((await broadcastIterator.next()).done, true);
}

async function testBroadcastRejectsResizedBufferedView() {
  const buffer = new ArrayBuffer(1, { maxByteLength: 2 });
  const { writer, broadcast: bc } = broadcast();
  const iterator = bc.push()[Symbol.asyncIterator]();

  assert.strictEqual(writer.writeSync(new Uint8Array(buffer)), true);
  buffer.resize(2);

  await assert.rejects(iterator.next(), kResizeError);
  await assert.rejects(writer.end(), kResizeError);
}

async function testShareRejectsResizedBufferedView() {
  const asyncBuffer = new ArrayBuffer(1, { maxByteLength: 2 });
  const shared = share([[new Uint8Array(asyncBuffer)]]);
  const first = shared.pull()[Symbol.asyncIterator]();
  const second = shared.pull()[Symbol.asyncIterator]();

  assert.strictEqual((await first.next()).done, false);
  asyncBuffer.resize(2);
  await assert.rejects(second.next(), kResizeError);

  const syncBuffer = new ArrayBuffer(1, { maxByteLength: 2 });
  const sharedSync = shareSync([[new Uint8Array(syncBuffer)]]);
  const firstSync = sharedSync.pull()[Symbol.iterator]();
  const secondSync = sharedSync.pull()[Symbol.iterator]();

  assert.strictEqual(firstSync.next().done, false);
  syncBuffer.resize(2);
  assert.throws(() => secondSync.next(), kResizeError);
}

async function testConsumersRejectResizedViews() {
  const asyncBuffer = new ArrayBuffer(1, { maxByteLength: 2 });
  async function* asyncSource() {
    yield [new Uint8Array(asyncBuffer)];
    asyncBuffer.resize(2);
  }
  await assert.rejects(array(asyncSource(), { limit: 1 }), kResizeError);

  const syncBuffer = new ArrayBuffer(1, { maxByteLength: 2 });
  function* syncSource() {
    yield [new Uint8Array(syncBuffer)];
    syncBuffer.resize(2);
  }
  assert.throws(() => arraySync(syncSource(), { limit: 1 }), kResizeError);
}

async function testPipeRejectsWriterResize() {
  const asyncBuffer = new ArrayBuffer(1, { maxByteLength: 2 });
  const asyncWriter = {
    write() {
      asyncBuffer.resize(2);
    },
    fail: common.mustCall(),
  };
  await assert.rejects(
    pipeTo([new Uint8Array(asyncBuffer)], asyncWriter),
    kResizeError,
  );

  const syncBuffer = new ArrayBuffer(1, { maxByteLength: 2 });
  const syncWriter = {
    writeSync() {
      syncBuffer.resize(2);
      return true;
    },
    fail: common.mustCall(),
  };
  assert.throws(
    () => pipeToSync(
      [new Uint8Array(syncBuffer)], syncWriter, { preventClose: true }),
    kResizeError,
  );
}

Promise.all([
  testBufferedViewMutationRejected(),
  testDropOldestUsesAcceptedByteLength(),
  testPendingWritesRejectResizedViews(),
  testBroadcastRejectsResizedBufferedView(),
  testShareRejectsResizedBufferedView(),
  testConsumersRejectResizedViews(),
  testPipeRejectsWriterResize(),
]).then(common.mustCall());
