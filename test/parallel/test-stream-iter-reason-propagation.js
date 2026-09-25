// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  Broadcast,
  broadcast,
  from,
  fromSync,
  pipeTo,
  pipeToSync,
  pull,
  push,
  share,
  shareSync,
} = require('stream/iter');

const reasons = [undefined, null, false, 0, '', 'failure'];

async function rejectsWith(promise, expected) {
  await assert.rejects(promise, (reason) => reason === expected);
}

function throwsWith(fn, expected) {
  assert.throws(fn, (reason) => reason === expected);
}

function asyncThrowingSource(reason) {
  return {
    __proto__: null,
    [Symbol.asyncIterator]() {
      return {
        __proto__: null,
        next() { return Promise.reject(reason); },
      };
    },
  };
}

function syncThrowingSource(reason) {
  return {
    __proto__: null,
    [Symbol.iterator]() {
      return {
        __proto__: null,
        next() { throw reason; },
      };
    },
  };
}

async function testPipeReasons() {
  for (const reason of reasons) {
    let asyncFailCalled = false;
    let asyncFailReason;
    const asyncWriter = {
      __proto__: null,
      write() {},
      fail(error) { asyncFailCalled = true; asyncFailReason = error; },
    };
    const asyncSource = asyncThrowingSource(reason);

    await rejectsWith(pipeTo(asyncSource, asyncWriter), reason);
    assert.strictEqual(asyncFailCalled, true);
    assert.strictEqual(asyncFailReason, reason);

    let syncFailCalled = false;
    let syncFailReason;
    const syncWriter = {
      __proto__: null,
      writeSync() { return true; },
      endSync() { return 0; },
      fail(error) { syncFailCalled = true; syncFailReason = error; },
    };
    const syncSource = syncThrowingSource(reason);

    throwsWith(() => pipeToSync(syncSource, syncWriter), reason);
    assert.strictEqual(syncFailCalled, true);
    assert.strictEqual(syncFailReason, reason);
  }
}

async function testSharedSourceReasons() {
  for (const reason of reasons) {
    const asyncSource = asyncThrowingSource(reason);
    const asyncIterator = share(asyncSource).pull()[Symbol.asyncIterator]();
    await rejectsWith(asyncIterator.next(), reason);

    const syncSource = syncThrowingSource(reason);
    const syncIterator = shareSync(syncSource).pull()[Symbol.iterator]();
    throwsWith(() => syncIterator.next(), reason);
  }
}

async function testBroadcastFromReason() {
  const reason = 'source failure';
  const source = asyncThrowingSource(reason);
  const { broadcast: channel } = Broadcast.from(source);
  await rejectsWith(channel.push()[Symbol.asyncIterator]().next(), reason);
}

async function testWriterFailReasons() {
  for (const reason of reasons) {
    const pushed = push();
    pushed.writer.fail(reason);
    await rejectsWith(
      pushed.readable[Symbol.asyncIterator]().next(), reason);
    await rejectsWith(pushed.writer.write('data'), reason);
    await rejectsWith(pushed.writer.end(), reason);

    const broadcasted = broadcast();
    const iterator = broadcasted.broadcast.push()[Symbol.asyncIterator]();
    const pendingRead = iterator.next();
    broadcasted.writer.fail(reason);
    await rejectsWith(pendingRead, reason);
    await rejectsWith(broadcasted.writer.write('data'), reason);
    await rejectsWith(broadcasted.writer.end(), reason);
    await rejectsWith(
      broadcasted.broadcast.push()[Symbol.asyncIterator]().next(), reason);
  }
}

async function testDisposeFailsWithUndefined() {
  const pushed = push();
  pushed.writer[Symbol.dispose]();
  await rejectsWith(
    pushed.readable[Symbol.asyncIterator]().next(), undefined);

  const broadcasted = broadcast();
  const next = broadcasted.broadcast
    .push()[Symbol.asyncIterator]().next();
  broadcasted.writer[Symbol.dispose]();
  await rejectsWith(next, undefined);
}

async function testExplicitUndefinedCancellation() {
  const broadcasted = broadcast();
  const broadcastNext = broadcasted.broadcast
    .push()[Symbol.asyncIterator]().next();
  broadcasted.broadcast.cancel(undefined);
  await rejectsWith(broadcastNext, undefined);

  const shared = share(from('data'));
  const shareIterator = shared.pull()[Symbol.asyncIterator]();
  shared.cancel(undefined);
  await rejectsWith(shareIterator.next(), undefined);

  const syncShared = shareSync(fromSync('data'));
  const syncIterator = syncShared.pull()[Symbol.iterator]();
  syncShared.cancel(undefined);
  throwsWith(() => syncIterator.next(), undefined);
}

async function testPendingWriteAbortReasons() {
  const chunk = new Uint8Array(16384);

  const pushed = push({ budget: chunk.byteLength });
  pushed.writer.writeSync(chunk);
  const pushController = new AbortController();
  const pushWrite = pushed.writer.write('data', {
    signal: pushController.signal,
  });
  pushController.abort(null);
  await rejectsWith(pushWrite, null);

  const broadcasted = broadcast({ budget: chunk.byteLength });
  broadcasted.broadcast.push();
  broadcasted.writer.writeSync(chunk);
  const broadcastController = new AbortController();
  const broadcastWrite = broadcasted.writer.write('data', {
    signal: broadcastController.signal,
  });
  broadcastController.abort(null);
  await rejectsWith(broadcastWrite, null);
  broadcasted.broadcast.cancel();
}

async function testTransformReasons() {
  for (const thrownReason of [undefined, 'transform failure']) {
    let observedReason;
    const watchSignal = (batch, { signal }) => {
      signal.addEventListener('abort', () => {
        observedReason = signal.reason;
      }, { once: true });
      return batch;
    };
    const throwReason = () => { throw thrownReason; };
    const transformed = pull(from('data'), watchSignal, throwReason);
    const iterator = transformed[Symbol.asyncIterator]();
    await rejectsWith(iterator.next(), thrownReason);
    assert.strictEqual(observedReason, thrownReason);
  }

  const controller = new AbortController();
  const started = Promise.withResolvers();
  const waitForAbort = (batch, { signal }) => {
    started.resolve();
    const { promise, reject } = Promise.withResolvers();
    signal.addEventListener('abort', () => reject(signal.reason), {
      once: true,
    });
    return promise;
  };
  const abortedIterator = pull(from('data'), waitForAbort, {
    signal: controller.signal,
  })[Symbol.asyncIterator]();
  const next = abortedIterator.next();
  await started.promise;
  controller.abort(null);
  await rejectsWith(next, null);
}

async function testIteratorThrowReasonReachesTransforms() {
  const reason = undefined;
  let observedReason;
  const transformed = pull(from('data'), (batch, { signal }) => {
    signal.addEventListener('abort', () => {
      observedReason = signal.reason;
    }, { once: true });
    return batch;
  });
  const iterator = transformed[Symbol.asyncIterator]();

  await iterator.next();
  await rejectsWith(iterator.throw(reason), reason);
  assert.strictEqual(observedReason, reason);
}

async function testErroredWriterPrecedesOperationSignal() {
  const failure = 0;
  const consumerReason = 'consumer failure';
  const signal = AbortSignal.abort(null);
  const { writer, readable } = push();
  const iterator = readable[Symbol.asyncIterator]();

  writer.fail(failure);

  await rejectsWith(writer.write('data', { signal }), failure);
  await rejectsWith(writer.end({ signal }), failure);
  await rejectsWith(iterator.throw(consumerReason), consumerReason);
  await rejectsWith(writer.write('data'), failure);
}

async function testCompletedBroadcastConsumerStaysCompleted() {
  const { writer, broadcast: channel } = broadcast();
  const iterator = channel.push()[Symbol.asyncIterator]();

  await iterator.return();
  writer.fail(undefined);

  assert.deepStrictEqual(await iterator.next(), {
    __proto__: null,
    done: true,
    value: undefined,
  });
}

Promise.all([
  testPipeReasons(),
  testSharedSourceReasons(),
  testBroadcastFromReason(),
  testWriterFailReasons(),
  testDisposeFailsWithUndefined(),
  testExplicitUndefinedCancellation(),
  testPendingWriteAbortReasons(),
  testTransformReasons(),
  testIteratorThrowReasonReachesTransforms(),
  testErroredWriterPrecedesOperationSignal(),
  testCompletedBroadcastConsumerStaysCompleted(),
]).then(common.mustCall());
