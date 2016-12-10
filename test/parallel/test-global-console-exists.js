/* eslint-disable required-modules */
// ordinarily test files must require('common') but that action causes
// the global console to be compiled, defeating the purpose of this test

'use strict';

const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const leak_warning = /EventEmitter memory leak detected\. 2 hello listeners/;

var write_calls = 0;

process.on('warning', (warning) => {
  // This will be called after the default internal
  // process warning handler is called. The default
  // process warning writes to the console, which will
  // invoke the monkeypatched process.stderr.write
  // below.
  assert.strictEqual(write_calls, 1);
  EventEmitter.defaultMaxListeners = old_default;
  // when we get here, we should be done
});

process.stderr.write = (data) => {
  if (write_calls === 0)
    assert.ok(data.match(leak_warning));
  else
    common.fail('stderr.write should be called only once');

  write_calls++;
};

const old_default = EventEmitter.defaultMaxListeners;
EventEmitter.defaultMaxListeners = 1;

const e = new EventEmitter();
e.on('hello', () => {});
e.on('hello', () => {});

// TODO: Figure out how to validate console. Currently,
// there is no obvious way of validating that console
// exists here exactly when it should.
