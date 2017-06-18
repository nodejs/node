/* eslint-disable required-modules */
// ordinarily test files must require('common') but that action causes
// the global console to be compiled, defeating the purpose of this test

'use strict';

const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const leakWarning = /EventEmitter memory leak detected\. 2 hello listeners/;

common.hijackStderr(common.mustCall(function(data) {
  if (process.stderr.writeTimes === 0) {
    assert.ok(leakWarning.test(data));
  } else {
    assert.fail('stderr.write should be called only once');
  }
}));

process.on('warning', function(warning) {
  // This will be called after the default internal
  // process warning handler is called. The default
  // process warning writes to the console, which will
  // invoke the monkeypatched process.stderr.write
  // below.
  assert.strictEqual(process.stderr.writeTimes, 1);
  EventEmitter.defaultMaxListeners = oldDefault;
  // when we get here, we should be done
});

const oldDefault = EventEmitter.defaultMaxListeners;
EventEmitter.defaultMaxListeners = 1;

const e = new EventEmitter();
e.on('hello', common.noop);
e.on('hello', common.noop);

// TODO: Figure out how to validate console. Currently,
// there is no obvious way of validating that console
// exists here exactly when it should.
