var assert = require('assert');
var common = require('../common');

var stream = require('stream');

var readable = new stream.Readable;

readable._read = function(){};

assert.ok(!readable.isFlowing());

// make the stream start flowing...
readable.on('data', function(){});

assert.ok(readable.isFlowing());

readable.pause();
assert.ok(!readable.isFlowing());
readable.resume();
assert.ok(readable.isFlowing());
