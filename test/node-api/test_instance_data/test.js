'use strict';
// Flags: --expose-gc

const common = require('../../common');
const test_instance_data =
  require(`./build/${common.buildType}/test_instance_data`);

// Test that instance data can be used in an async work callback.
new Promise((resolve) => test_instance_data.asyncWorkCallback(resolve))

  // Test that the buffer finalizer can access the instance data.
  .then(() => new Promise((resolve) => {
    test_instance_data.testBufferFinalizer(common.mustCall(resolve));
    global.gc();
  }))

  // Test that the thread-safe function can access the instance data.
  .then(() => new Promise((resolve) =>
    test_instance_data.testThreadsafeFunction(common.mustCall(),
                                              common.mustCall(resolve))));
