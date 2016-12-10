'use strict';
const common = require('../common');
var assert = require('assert');
var Readable = require('_stream_readable');

var len = 0;
var chunks = new Array(10);
for (var i = 1; i <= 10; i++) {
  chunks[i - 1] = Buffer.allocUnsafe(i);
  len += i;
}

var test = new Readable();
var n = 0;
test._read = function(size) {
  var chunk = chunks[n++];
  setTimeout(function() {
    test.push(chunk === undefined ? null : chunk);
  });
};

test.on('end', thrower);
function thrower() {
  throw new Error('this should not happen!');
}

var bytesread = 0;
test.on('readable', function() {
  var b = len - bytesread - 1;
  var res = test.read(b);
  if (res) {
    bytesread += res.length;
    console.error('br=%d len=%d', bytesread, len);
    setTimeout(next);
  }
  test.read(0);
});
test.read(0);

function next() {
  // now let's make 'end' happen
  test.removeListener('end', thrower);
  test.on('end', common.mustCall(function() {}));

  // one to get the last byte
  var r = test.read();
  assert(r);
  assert.equal(r.length, 1);
  r = test.read();
  assert.equal(r, null);
}
