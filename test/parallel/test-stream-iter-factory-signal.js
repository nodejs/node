// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { getEventListeners } = require('events');
const {
  broadcast,
  duplex,
  push,
  share,
  text,
} = require('stream/iter');

function abortListenerCount(signal) {
  return getEventListeners(signal, 'abort').length;
}

async function testPushSignalLifetime() {
  const controller = new AbortController();
  const { writer, readable } = push({ signal: controller.signal });
  const iterator = readable[Symbol.asyncIterator]();

  assert.strictEqual(abortListenerCount(controller.signal), 1);
  assert.strictEqual(writer.writeSync('x'), true);
  const ending = writer.end();
  assert.strictEqual(abortListenerCount(controller.signal), 1);

  assert.strictEqual((await iterator.next()).done, false);
  assert.strictEqual((await iterator.next()).done, true);
  assert.strictEqual(await ending, 1);
  assert.strictEqual(abortListenerCount(controller.signal), 0);
}

async function testBroadcastSignalLifetime() {
  const controller = new AbortController();
  const { writer, broadcast: shared } = broadcast({
    signal: controller.signal,
  });
  const iterator = shared.push()[Symbol.asyncIterator]();

  assert.strictEqual(abortListenerCount(controller.signal), 1);
  assert.strictEqual(writer.writeSync('x'), true);
  const ending = writer.end();
  assert.strictEqual(abortListenerCount(controller.signal), 1);

  assert.strictEqual((await iterator.next()).done, false);
  assert.strictEqual((await iterator.next()).done, true);
  assert.strictEqual(await ending, 1);
  assert.strictEqual(abortListenerCount(controller.signal), 0);
}

async function testShareSignalLifetime() {
  const controller = new AbortController();
  const shared = share('x', { signal: controller.signal });
  const iterator = shared.pull()[Symbol.asyncIterator]();

  assert.strictEqual(abortListenerCount(controller.signal), 1);
  assert.strictEqual((await iterator.next()).done, false);
  assert.strictEqual(abortListenerCount(controller.signal), 1);
  assert.strictEqual((await iterator.next()).done, true);
  assert.strictEqual(abortListenerCount(controller.signal), 0);
}

async function testDuplexSignalLifetime() {
  const controller = new AbortController();
  const [channelA, channelB] = duplex({ signal: controller.signal });

  assert.strictEqual(abortListenerCount(controller.signal), 1);
  await channelA.writer.write('x');
  const closing = channelA.close();
  assert.strictEqual(abortListenerCount(controller.signal), 1);

  assert.strictEqual(await text(channelB.readable), 'x');
  await closing;
  assert.strictEqual(abortListenerCount(controller.signal), 0);
}

Promise.all([
  testPushSignalLifetime(),
  testBroadcastSignalLifetime(),
  testShareSignalLifetime(),
  testDuplexSignalLifetime(),
]).then(common.mustCall());
