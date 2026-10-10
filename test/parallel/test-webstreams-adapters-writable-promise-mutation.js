'use strict';

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');

async function checkCompletion(target, key, fail) {
  const started = Promise.withResolvers();
  let finishWrite;
  const error = fail ? new Error('native write failure') : undefined;
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      finishWrite = callback;
      started.resolve();
    }),
  });
  const writer = Writable.toWeb(writable).getWriter();
  const write = writer.write(Buffer.from([1]));
  const completion = fail ? assert.rejects(write, (actual) => actual === error) : write;
  const closed = fail ? assert.rejects(writer.closed, (actual) => actual === error) : undefined;
  await started.promise;

  const descriptor = Object.getOwnPropertyDescriptor(target, key);
  Object.defineProperty(target, key, {
    __proto__: null,
    configurable: true,
    get: common.mustNotCall('completion must not look up the Promise constructor or species'),
  });
  try {
    // Registering the completion reaction later must not introduce a new
    // user-code call or exception inside the native write callback.
    finishWrite(error);
  } finally {
    Object.defineProperty(target, key, descriptor);
  }

  await completion;
  if (fail) {
    await closed;
  } else {
    await writer.close();
  }
}

async function checkReactionError() {
  const started = Promise.withResolvers();
  const observed = Promise.withResolvers();
  const error = new Error('write reaction failure');
  const descriptor = Object.getOwnPropertyDescriptor(Promise.prototype, 'constructor');
  let finishWrite;
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      if (chunk[0] === 1) {
        finishWrite = callback;
        started.resolve();
        return;
      }
      // Force the next synchronous sink reaction to throw while the first
      // asynchronous write's fulfillment reaction advances the queue.
      Object.defineProperty(Promise.prototype, 'constructor', {
        __proto__: null,
        configurable: true,
        get() { throw error; },
      });
      callback();
    }, 2),
  });
  const writer = Writable.toWeb(writable).getWriter();
  const first = writer.write(Buffer.from([1]));
  writer.write(Buffer.from([2])).catch(() => {});
  writer.closed.catch(() => {});
  const onUncaught = common.mustNotCall('a throwing write reaction must reject a promise');
  process.once('uncaughtException', onUncaught);
  process.once('unhandledRejection', common.mustCall((actual) => {
    Object.defineProperty(Promise.prototype, 'constructor', descriptor);
    assert.strictEqual(actual, error);
    writable.destroy();
    observed.resolve();
  }));
  await started.promise;
  finishWrite();
  await observed.promise;
  await first;
  process.removeListener('uncaughtException', onUncaught);
}

async function main() {
  for (const [target, key] of [
    [Promise.prototype, 'constructor'],
    [Promise, Symbol.species],
  ]) {
    for (const fail of [false, true]) {
      await checkCompletion(target, key, fail);
    }
  }
  await checkReactionError();
}

main().then(common.mustCall());
