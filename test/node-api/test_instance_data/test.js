'use strict';
// Test API calls for instance data.

const common = require('../../common');

if (module.parent) {
  // When required as a module, run the tests.
  const test_instance_data =
    require(`./build/${common.buildType}/test_instance_data`);

  // Test that instance data can be used in an async work callback.
  new Promise((resolve) => test_instance_data.asyncWorkCallback(resolve))

    // Test that the buffer finalizer can access the instance data.
    .then(() => new Promise((resolve) => {
      test_instance_data.testBufferFinalizer(resolve);
      global.gc();
    }))

    // Test that the thread-safe function can access the instance data.
    .then(() => new Promise((resolve) =>
      test_instance_data.testThreadsafeFunction(common.mustCall(),
                                                common.mustCall(resolve))));
} else {
  // When launched as a script, run tests in either a child process or in a
  // worker thread.
  const requireAs = require('../../common/require-as');
  const runOptions = { stdio: ['inherit', 'pipe', 'inherit'] };

  // Run tests in a child process.
  requireAs(__filename, ['--expose-gc'], runOptions, 'child');

  // Run tests in a worker thread in a child process.
  requireAs(__filename, ['--expose-gc'], runOptions, 'worker');
}
