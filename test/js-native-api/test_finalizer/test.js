'use strict';
// Flags: --expose-gc

const common = require('../../common');
const test_finalizer = require(`./build/${common.buildType}/test_finalizer`);
const assert = require('assert');

(() => {
  const obj = {};
  test_finalizer.addFinalizer(obj);
})();

for (let i = 0; i < 10; ++i) {
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
  await common.gcUntil('ensure JS finalizer called',
                       () => (test_finalizer.getFinalizerCallCount() === 2));
  assert(js_is_called);
}
runAsyncTests();
