'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const vm = require('vm');

// err1 is emitted sync as a control - no events
// err2 is emitted after a timeout - uncaughtExceptionMonitor
//                                 + uncaughtException
// err3 is emitted after some awaits -  unhandledRejection
// err4 is emitted during handling err3 - uncaughtExceptionMonitor
// err5 is emitted after err4 from a VM lacking hooks - unhandledRejection
//                                                    + uncaughtException

const asyncLocalStorage = new AsyncLocalStorage();
const callbackToken = { callbackToken: true };
const awaitToken = { awaitToken: true };

let i = 0;

// Redefining the uncaughtExceptionHandler is a bit odd, so we just do this
// so we can track total invocations
let underlyingExceptionHandler;
const exceptionHandler = common.mustCall(function(...args) {
  return underlyingExceptionHandler.call(this, ...args);
}, 2);
process.setUncaughtExceptionCaptureCallback(exceptionHandler);

const exceptionMonitor = common.mustCall((err, origin) => {
  if (err.message === 'err2') {
    assert.strictEqual(origin, 'uncaughtException');
    assert.strictEqual(asyncLocalStorage.getStore(), callbackToken);
  } else if (err.message === 'err4') {
    assert.strictEqual(origin, 'unhandledRejection');
    assert.strictEqual(asyncLocalStorage.getStore(), awaitToken);
  } else {
    assert.fail('unknown error ' + err);
  }
}, 2);
process.on('uncaughtExceptionMonitor', exceptionMonitor);

function fireErr1() {
  underlyingExceptionHandler = common.mustCall(function(err) {
    ++i;
    assert.strictEqual(err.message, 'err2');
    assert.strictEqual(asyncLocalStorage.getStore(), callbackToken);
  }, 1);
  try {
    asyncLocalStorage.run(callbackToken, () => {
      setTimeout(fireErr2, 0);
      throw new Error('err1');
    });
  } catch (e) {
    assert.strictEqual(e.message, 'err1');
    assert.strictEqual(asyncLocalStorage.getStore(), undefined);
  }
}

function fireErr2() {
  process.nextTick(() => {
    assert.strictEqual(i, 1);
    fireErr3();
  });
  throw new Error('err2');
}

function fireErr3() {
  assert.strictEqual(asyncLocalStorage.getStore(), callbackToken);
  const rejectionHandler3 = common.mustCall((err) => {
    assert.strictEqual(err.message, 'err3');
    assert.strictEqual(asyncLocalStorage.getStore(), awaitToken);
    process.off('unhandledRejection', rejectionHandler3);

    fireErr4();
  }, 1);
  process.on('unhandledRejection', rejectionHandler3);
  async function awaitTest() {
    await null;
    throw new Error('err3');
  }
  asyncLocalStorage.run(awaitToken, awaitTest);
}

const uncaughtExceptionHandler4 = common.mustCall(
  function(err) {
    assert.strictEqual(err.message, 'err4');
    assert.strictEqual(asyncLocalStorage.getStore(), awaitToken);
    fireErr5();
  }, 1);
function fireErr4() {
  assert.strictEqual(asyncLocalStorage.getStore(), awaitToken);
  underlyingExceptionHandler = uncaughtExceptionHandler4;
  // re-entrant check
  Promise.reject(new Error('err4'));
}

function fireErr5() {
  assert.strictEqual(asyncLocalStorage.getStore(), awaitToken);
  underlyingExceptionHandler = () => {};
  const rejectionHandler5 = common.mustCall((err) => {
    assert.strictEqual(err.message, 'err5');
    assert.strictEqual(asyncLocalStorage.getStore(), awaitToken);
    process.off('unhandledRejection', rejectionHandler5);
  }, 1);
  process.on('unhandledRejection', rejectionHandler5);
  const makeOrphan = vm.compileFunction(`(${String(() => {
    async function main() {
      await null;
      Promise.resolve().then(() => {
        throw new Error('err5');
      });
    }
    main();
  })})()`);
  makeOrphan();
}

fireErr1();
