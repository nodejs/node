'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('_stream_readable');

let len = 0;
const chunks = new Array(10);
for (let i = 1; i <= 10; i++) {
  chunks[i - 1] = Buffer.allocUnsafe(i);
  len += i;
}

const test = new Readable();
let n = 0;
test._read = function(size) {
  const chunk = chunks[n++];
  setTimeout(function() {
    test.push(chunk === undefined ? null : chunk);
  }, 1);
};

test.on('end', thrower);
function thrower() {
  throw new Error('this should not happen!');
}

let bytesread = 0;
test.on('readable', function() {
  const b = len - bytesread - 1;
  const res = test.read(b);
  if (res) {
    bytesread += res.length;
    console.error(`br=${bytesread} len=${len}`);
    setTimeout(next, 1);
  }
  test.read(0);
});
test.read(0);

function next() {
  // now let's make 'end' happen
  test.removeListener('end', thrower);
  test.on('end', common.mustCall());

  // one to get the last byte
  let r = test.read();
  assert(r);
  assert.strictEqual(r.length, 1);
  r = test.read();
  assert.strictEqual(r, null);
}
