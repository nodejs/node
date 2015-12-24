'use strict';
require('../common');
var assert = require('assert');
var stream = require('stream');
var str = 'asdfasdfasdfasdfasdf';

var r = new stream.Readable({
  highWaterMark: 5,
  encoding: 'utf8'
});

var reads = 0;
var eofed = false;
var ended = false;

r._read = function(n) {
  if (reads === 0) {
    setTimeout(function() {
      r.push(str);
    });
    reads++;
  } else if (reads === 1) {
    var ret = r.push(str);
    assert.equal(ret, false);
    reads++;
  } else {
    assert(!eofed);
    eofed = true;
    r.push(null);
  }
};

r.on('end', function() {
  ended = true;
});

// push some data in to start.
// we've never gotten any read event at this point.
var ret = r.push(str);
// should be false.  > hwm
assert(!ret);
var chunk = r.read();
assert.equal(chunk, str);
chunk = r.read();
assert.equal(chunk, null);

r.once('readable', function() {
  // this time, we'll get *all* the remaining data, because
  // it's been added synchronously, as the read WOULD take
  // us below the hwm, and so it triggered a _read() again,
  // which synchronously added more, which we then return.
  chunk = r.read();
  assert.equal(chunk, str + str);

  chunk = r.read();
  assert.equal(chunk, null);
});

process.on('exit', function() {
  assert(eofed);
  assert(ended);
  assert.equal(reads, 2);
  console.log('ok');
});
