'use strict';
var common = require('../common');
var assert = require('assert');

var Transform = require('stream').Transform;

var parser = new Transform({ readableObjectMode : true });

assert(parser._readableState.objectMode);
assert(!parser._writableState.objectMode);
assert(parser._readableState.highWaterMark === 16);
assert(parser._writableState.highWaterMark === (16 * 1024));

parser._transform = function(chunk, enc, callback) {
  callback(null, { val : chunk[0] });
};

var parsed;

parser.on('data', function(obj) {
  parsed = obj;
});

parser.end(new Buffer([42]));

process.on('exit', function() {
  assert(parsed.val === 42);
});


var serializer = new Transform({ writableObjectMode : true });

assert(!serializer._readableState.objectMode);
assert(serializer._writableState.objectMode);
assert(serializer._readableState.highWaterMark === (16 * 1024));
assert(serializer._writableState.highWaterMark === 16);

serializer._transform = function(obj, _, callback) {
  callback(null, new Buffer([obj.val]));
};

var serialized;

serializer.on('data', function(chunk) {
  serialized = chunk;
});

serializer.write({ val : 42 });

process.on('exit', function() {
  assert(serialized[0] === 42);
});
