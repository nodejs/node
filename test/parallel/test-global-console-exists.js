/* eslint-disable required-modules */
// ordinarily test files must require('common') but that action causes
// the global console to be compiled, defeating the purpose of this test

'use strict';

const assert = require('assert');
const events = require('events');
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

const old_default = events.defaultMaxListeners;
events.defaultMaxListeners = 1;

const e = new events.EventEmitter();
e.on('hello', function() {});
e.on('hello', function() {});

assert.equal(write_calls, 2);

events.defaultMaxListeners = old_default;
