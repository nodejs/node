'use strict';
require('../common');
var assert = require('assert');

var stream = require('stream');

var readable = new stream.Readable();

// _read is a noop, here.
readable._read = Function();

// default state of a stream is not "paused"
assert.ok(!readable.isPaused());

// make the stream start flowing...
readable.on('data', Function());

// still not paused.
assert.ok(!readable.isPaused());

readable.pause();
assert.ok(readable.isPaused());
readable.resume();
assert.ok(!readable.isPaused());
