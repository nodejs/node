'use strict';
require('../common');
const assert = require('assert');

const Transform = require('stream').Transform;

const parser = new Transform({ readableObjectMode: true });

assert(parser._readableState.objectMode);
assert(!parser._writableState.objectMode);
assert.strictEqual(parser._readableState.highWaterMark, 16);
assert.strictEqual(parser._writableState.highWaterMark, 16 * 1024);

parser._transform = function(chunk, enc, callback) {
  callback(null, { val: chunk[0] });
};

let parsed;

parser.on('data', function(obj) {
  parsed = obj;
});

parser.end(Buffer.from([42]));

process.on('exit', function() {
  assert.strictEqual(parsed.val, 42);
});


const serializer = new Transform({ writableObjectMode: true });

assert(!serializer._readableState.objectMode);
assert(serializer._writableState.objectMode);
assert.strictEqual(serializer._readableState.highWaterMark, 16 * 1024);
assert.strictEqual(serializer._writableState.highWaterMark, 16);

serializer._transform = function(obj, _, callback) {
  callback(null, Buffer.from([obj.val]));
};

let serialized;

serializer.on('data', function(chunk) {
  serialized = chunk;
});

serializer.write({ val: 42 });

process.on('exit', function() {
  assert.strictEqual(serialized[0], 42);
});
