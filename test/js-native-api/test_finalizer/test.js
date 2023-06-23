'use strict';
// Flags: --expose-gc

const common = require('../../common');
const test_finalizer = require(`./build/${common.buildType}/test_finalizer`);
const assert = require('assert');

function addFinalizer() {
  // Define obj in a function context to let it GC-collected.
  const obj = {};
  test_finalizer.addFinalizer(obj);
}

addFinalizer();

for (let i = 0; i < 1000; ++i) {
  global.gc();
  if (test_finalizer.getFinalizerCallCount() === 1) {
    break;
  }
}

assert(test_finalizer.getFinalizerCallCount() === 1);

async function runAsyncTests() {
  let js_is_called = false;
  (() => {
    const obj = {};
    test_finalizer.addFinalizerWithJS(obj, () => { js_is_called = true; });
  })();
  await common.gcUntil('test JS finalizer',
                       () => (test_finalizer.getFinalizerCallCount() === 2));
  assert(js_is_called);
}
runAsyncTests();
