'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const { fork } = require('child_process');
const expectedArray = (function(arrayLength) {
  const result = [];
  for (let index = 0; index < arrayLength; index++) {
    result.push(arrayLength - 1 - index);
  }
  return result;
})(binding.ARRAY_LENGTH);

// Handle the rapid teardown test case as the child process. We unref the
// thread-safe function after we have received two values. This causes the
// process to exit and the environment cleanup handler to be invoked.
if (process.argv[2] === 'child') {
  let callCount = 0;
  binding.StartThread((value) => {
    callCount++;
    console.log(value);
    if (callCount === 2) {
      binding.Unref();
    }
  }, false /* abort */, true /* launchSecondary */);

  // Release the thread-safe function from the main thread so that it may be
  // torn down via the environment cleanup handler.
  binding.Release();
  return;
}

function testWithJSMarshaller({
  threadStarter,
  quitAfter,
  abort,
  launchSecondary }) {
  return new Promise((resolve) => {
    const array = [];
    binding[threadStarter](function testCallback(value) {
      array.push(value);
      if (array.length === quitAfter) {
        setImmediate(() => {
          binding.StopThread(common.mustCall(() => {
            resolve(array);
          }), !!abort);
        });
      }
    }, !!abort, !!launchSecondary);
    if (threadStarter === 'StartThreadNonblocking') {
      // Let's make this thread really busy for a short while to ensure that
      // the queue fills and the thread receives a napi_queue_full.
      const start = Date.now();
      while (Date.now() - start < 200);
    }
  });
}

new Promise(function testWithoutJSMarshaller(resolve) {
  let callCount = 0;
  binding.StartThreadNoNative(function testCallback() {
    callCount++;

    // The default call-into-JS implementation passes no arguments.
    assert.strictEqual(arguments.length, 0);
    if (callCount === binding.ARRAY_LENGTH) {
      setImmediate(() => {
        binding.StopThread(common.mustCall(() => {
          resolve();
        }), false);
      });
    }
  }, false /* abort */, false /* launchSecondary */);
})

// Start the thread in blocking mode, and assert that all values are passed.
// Quit after it's done.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  quitAfter: binding.ARRAY_LENGTH
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in non-blocking mode, and assert that all values are passed.
// Quit after it's done.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  quitAfter: binding.ARRAY_LENGTH
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in blocking mode, and assert that all values are passed.
// Quit early, but let the thread finish.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  quitAfter: 1
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in non-blocking mode, and assert that all values are passed.
// Quit early, but let the thread finish.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  quitAfter: 1
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in blocking mode, and assert that all values are passed.
// Quit early, but let the thread finish. Launch a secondary thread to test the
// reference counter incrementing functionality.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  quitAfter: 1,
  launchSecondary: true
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in non-blocking mode, and assert that all values are passed.
// Quit early, but let the thread finish. Launch a secondary thread to test the
// reference counter incrementing functionality.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  quitAfter: 1,
  launchSecondary: true
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in blocking mode, and assert that it could not finish.
// Quit early and aborting.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  quitAfter: 1,
  abort: true
}))
.then((result) => assert.strictEqual(result.indexOf(0), -1))

// Start the thread in non-blocking mode, and assert that it could not finish.
// Quit early and aborting.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  quitAfter: 1,
  abort: true
}))
.then((result) => assert.strictEqual(result.indexOf(0), -1))

// Start a child process to test rapid teardown
.then(() => {
  return new Promise((resolve, reject) => {
    let output = '';
    const child = fork(__filename, ['child'], {
      stdio: [process.stdin, 'pipe', process.stderr, 'ipc']
    });
    child.on('close', (code) => {
      if (code === 0) {
        resolve(output.match(/\S+/g));
      } else {
        reject(new Error('Child process died with code ' + code));
      }
    });
    child.stdout.on('data', (data) => (output += data.toString()));
  });
})
.then((result) => assert.strictEqual(result.indexOf(0), -1));
