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
  }, false /* abort */, true /* launchSecondary */, +process.argv[3]);

  // Release the thread-safe function from the main thread so that it may be
  // torn down via the environment cleanup handler.
  binding.Release();
  return;
}

function testWithJSMarshaller({
  threadStarter,
  quitAfter,
  abort,
  maxQueueSize,
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
    }, !!abort, !!launchSecondary, maxQueueSize);
    if (threadStarter === 'StartThreadNonblocking') {
      // Let's make this thread really busy for a short while to ensure that
      // the queue fills and the thread receives a napi_queue_full.
      const start = Date.now();
      while (Date.now() - start < 200);
    }
  });
}

function testUnref(queueSize) {
  return new Promise((resolve, reject) => {
    let output = '';
    const child = fork(__filename, ['child', queueSize], {
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
  })
  .then((result) => assert.strictEqual(result.indexOf(0), -1));
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
  }, false /* abort */, false /* launchSecondary */, binding.MAX_QUEUE_SIZE);
})

// Start the thread in blocking mode, and assert that all values are passed.
// Quit after it's done.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  quitAfter: binding.ARRAY_LENGTH
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in blocking mode, and assert that all values are passed.
// Quit after it's done.
// Doesn't pass the callback js function to napi_create_threadsafe_function.
// Instead, use an alternative reference to get js function called.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNoJsFunc',
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  quitAfter: binding.ARRAY_LENGTH
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in blocking mode with an infinite queue, and assert that all
// values are passed. Quit after it's done.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  maxQueueSize: 0,
  quitAfter: binding.ARRAY_LENGTH
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in non-blocking mode, and assert that all values are passed.
// Quit after it's done.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  quitAfter: binding.ARRAY_LENGTH
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in blocking mode, and assert that all values are passed.
// Quit early, but let the thread finish.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  quitAfter: 1
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in blocking mode with an infinite queue, and assert that all
// values are passed. Quit early, but let the thread finish.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  maxQueueSize: 0,
  quitAfter: 1
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in non-blocking mode, and assert that all values are passed.
// Quit early, but let the thread finish.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  quitAfter: 1
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in blocking mode, and assert that all values are passed.
// Quit early, but let the thread finish. Launch a secondary thread to test the
// reference counter incrementing functionality.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  quitAfter: 1,
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  launchSecondary: true
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in non-blocking mode, and assert that all values are passed.
// Quit early, but let the thread finish. Launch a secondary thread to test the
// reference counter incrementing functionality.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  quitAfter: 1,
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  launchSecondary: true
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start the thread in blocking mode, and assert that it could not finish.
// Quit early by aborting.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  quitAfter: 1,
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  abort: true
}))
.then((result) => assert.strictEqual(result.indexOf(0), -1))

// Start the thread in blocking mode with an infinite queue, and assert that it
// could not finish. Quit early by aborting.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  quitAfter: 1,
  maxQueueSize: 0,
  abort: true
}))
.then((result) => assert.strictEqual(result.indexOf(0), -1))

// Start the thread in non-blocking mode, and assert that it could not finish.
// Quit early and aborting.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  quitAfter: 1,
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  abort: true
}))
.then((result) => assert.strictEqual(result.indexOf(0), -1))

// Start a child process to test rapid teardown.
.then(() => testUnref(binding.MAX_QUEUE_SIZE))

// Start a child process with an infinite queue to test rapid teardown.
.then(() => testUnref(0))

// Test deadlock prevention.
.then(() => assert.deepStrictEqual(binding.TestDeadlock(), {
  deadlockTest: 'Main thread would deadlock'
}));
