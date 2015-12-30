/* eslint-disable required-modules */
// ordinarily test files must require('common') but that action causes
// the global console to be compiled, defeating the purpose of this test

'use strict';

const assert = require('assert');
const EventEmitter = require('events');
const leak_warning = /EventEmitter memory leak detected\. 2 hello listeners/;

var write_calls = 0;
process.stderr.write = function(data) {
  if (write_calls === 0)
    assert.ok(data.match(leak_warning));
  else if (write_calls === 1)
    assert.ok(data.match(/Trace/));
  else
    assert.ok(false, 'stderr.write should be called only twice');

  write_calls++;
};

const old_default = EventEmitter.defaultMaxListeners;
EventEmitter.defaultMaxListeners = 1;

const e = new EventEmitter();
e.on('hello', function() {});
e.on('hello', function() {});

// TODO: figure out how to validate console. Currently,
// there is no obvious way of validating that console
// exists here exactly when it should.

assert.equal(write_calls, 2);

EventEmitter.defaultMaxListeners = old_default;
