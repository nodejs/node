'use strict';

const common = require('./');
const { once } = require('node:events');
const { createPipe } = require('node:net');
const { test } = require('node:test');

const kPipeCloseTimeout = common.platformTimeout(10_000);

function isClosed(stream) {
  return stream.closed || stream.destroyed;
}

function waitForClose(stream) {
  if (isClosed(stream))
    return Promise.resolve();

  return once(stream, 'close');
}

async function assertPipeCloses(readable, writable) {
  const timeout = new Promise((_, reject) => {
    const timer = setTimeout(() => {
      reject(new Error('pipe endpoints did not close'));
    }, kPipeCloseTimeout);
    timer.unref();
  });

  await Promise.race([
    Promise.all([
      waitForClose(readable),
      waitForClose(writable),
    ]),
    timeout,
  ]);
}

async function withCreatePipe(fn) {
  const pipe = createPipe();

  try {
    const result = await fn(pipe);
    await assertPipeCloses(pipe.readable, pipe.writable);
    return result;
  } finally {
    pipe.readable.destroy();
    pipe.writable.destroy();
  }
}

function testCreatePipe(name, fn) {
  test(name, (t) => withCreatePipe(({ readable, writable }) => {
    return fn(readable, writable, t);
  }));
}

module.exports = {
  testCreatePipe,
  withCreatePipe,
};
