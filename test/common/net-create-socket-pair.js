'use strict';

const common = require('./');
const { once } = require('node:events');
const { createSocketPair } = require('node:net');
const { test } = require('node:test');

const kSocketPairCloseTimeout = common.platformTimeout(10_000);

function isClosed(stream) {
  return stream.closed || stream.destroyed;
}

function waitForClose(stream) {
  if (isClosed(stream))
    return Promise.resolve();

  return once(stream, 'close');
}

async function assertSocketPairCloses(left, right) {
  const timeout = new Promise((_, reject) => {
    const timer = setTimeout(() => {
      reject(new Error('socket pair endpoints did not close'));
    }, kSocketPairCloseTimeout);
    timer.unref();
  });

  await Promise.race([
    Promise.all([
      waitForClose(left),
      waitForClose(right),
    ]),
    timeout,
  ]);
}

async function withCreateSocketPair(fn) {
  const [left, right] = createSocketPair();

  try {
    const result = await fn({ left, right });
    await assertSocketPairCloses(left, right);
    return result;
  } finally {
    left.destroy();
    right.destroy();
  }
}

function testCreateSocketPair(name, options, fn) {
  if (fn === undefined) {
    fn = options;
    options = undefined;
  }

  test(name, options, (t) => withCreateSocketPair(({ left, right }) => {
    return fn(left, right, t);
  }));
}

module.exports = {
  testCreateSocketPair,
  withCreateSocketPair,
};
