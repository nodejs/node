'use strict';

const common = require('../../common');
const { start, expectedArray, testWithJSMarshaller, testUnref } = require('./common-threadsafe');

const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);

// Start the thread in blocking mode with an infinite queue, and assert that all
// values are passed. Quit after it's done.
start.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  maxQueueSize: 0,
  quitAfter: binding.ARRAY_LENGTH
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

// Start the thread in blocking mode with an infinite queue, and assert that it
// could not finish. Quit early by aborting.
.then(() => testWithJSMarshaller({
  threadStarter: 'StartThread',
  quitAfter: 1,
  maxQueueSize: 0,
  abort: true
}))
.then((result) => assert.strictEqual(result.indexOf(0), -1))

// Start a child process with an infinite queue to test rapid teardown
.then(() => testUnref(0));
