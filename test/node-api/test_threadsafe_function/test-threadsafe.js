'use strict';

const common = require('../../common');
const { start, expectedArray, testWithJSMarshaller, testUnref } = require('./common-threadsafe');

const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);

// Start the thread in blocking mode, and assert that all values are passed.
// Quit after it's done.
start.then(() => testWithJSMarshaller({
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

// Start the thread in non-blocking mode, and assert that it could not finish.
// Quit early and aborting.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  quitAfter: 1,
  maxQueueSize: binding.MAX_QUEUE_SIZE,
  abort: true
}))
.then((result) => assert.strictEqual(result.indexOf(0), -1))

// Make sure that threadsafe function isn't stalled when we hit
// `kMaxIterationCount` in `src/node_api.cc`
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThreadNonblocking',
  maxQueueSize: binding.ARRAY_LENGTH >>> 1,
  quitAfter: binding.ARRAY_LENGTH
}))
.then((result) => assert.deepStrictEqual(result, expectedArray))

// Start a child process to test rapid teardown
.then(() => testUnref(binding.MAX_QUEUE_SIZE));
