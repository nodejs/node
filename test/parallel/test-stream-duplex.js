'use strict';
require('../common');
const assert = require('assert');
const Duplex = require('stream').Duplex;

const stream = new Duplex({ objectMode: true });

assert(Duplex() instanceof Duplex);
assert(stream._readableState.objectMode);
assert(stream._writableState.objectMode);

let written;
let read;

stream._write = (obj, _, cb) => {
  written = obj;
  cb();
};

stream._read = () => {};

stream.on('data', (obj) => {
  read = obj;
});

stream.push({ val: 1 });
stream.end({ val: 2 });

process.on('exit', () => {
  assert.strictEqual(read.val, 1);
  assert.strictEqual(written.val, 2);
});
