/* eslint-disable required-modules */

'use strict';

// Ordinarily test files must require('common') but that action causes
// the global console to be compiled, defeating the purpose of this test.

const assert = require('assert');
const EventEmitter = require('events');
const leakWarning = /EventEmitter memory leak detected\. 2 hello listeners/;

let writeTimes = 0;
let warningTimes = 0;
process.on('warning', () => {
  // This will be called after the default internal
  // process warning handler is called. The default
  // process warning writes to the console, which will
  // invoke the monkeypatched process.stderr.write
  // below.
  assert.strictEqual(writeTimes, 1);
  EventEmitter.defaultMaxListeners = oldDefault;
  warningTimes++;
});

process.on('exit', () => {
  assert.strictEqual(warningTimes, 1);
});

process.stderr.write = (data) => {
  if (writeTimes === 0)
    assert.ok(leakWarning.test(data));
  else
    assert.fail('stderr.write should be called only once');

  writeTimes++;
};

const oldDefault = EventEmitter.defaultMaxListeners;
EventEmitter.defaultMaxListeners = 1;

const e = new EventEmitter();
e.on('hello', () => {});
e.on('hello', () => {});

// TODO: Figure out how to validate console. Currently,
// there is no obvious way of validating that console
// exists here exactly when it should.
