// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

// This test verifies that:
// 1. unshift() does not cause colliding _read() calls.
// 2. unshift() after the 'end' event is an error, but after the EOF
//    signalling null, it is ok, and just creates a new readable chunk.
// 3. push() after the EOF signaling null is an error.
// 4. _read() is not called after pushing the EOF null chunk.

var stream = require('stream');
var hwm = 10;
var r = stream.Readable({ highWaterMark: hwm });
var chunks = 10;
var t = (chunks * 5);

var data = new Buffer(chunks * hwm + Math.ceil(hwm / 2));
for (var i = 0; i < data.length; i++) {
  var c = 'asdf'.charCodeAt(i % 4);
  data[i] = c;
}

var pos = 0;
var pushedNull = false;
r._read = function(n) {
  assert(!pushedNull, '_read after null push');

  // every third chunk is fast
  push(!(chunks % 3));

  function push(fast) {
    assert(!pushedNull, 'push() after null push');
    var c = pos >= data.length ? null : data.slice(pos, pos + n);
    pushedNull = c === null;
    if (fast) {
      pos += n;
      r.push(c);
      if (c === null) pushError();
    } else {
      setTimeout(function() {
        pos += n;
        r.push(c);
        if (c === null) pushError();
      });
    }
  }
};

function pushError() {
  assert.throws(function() {
    r.push(new Buffer(1));
  });
}


var w = stream.Writable();
var written = [];
w._write = function(chunk, encoding, cb) {
  written.push(chunk.toString());
  cb();
};

var ended = false;
r.on('end', function() {
  assert(!ended, 'end emitted more than once');
  assert.throws(function() {
    r.unshift(new Buffer(1));
  });
  ended = true;
  w.end();
});

r.on('readable', function() {
  var chunk;
  while (null !== (chunk = r.read(10))) {
    w.write(chunk);
    if (chunk.length > 4)
      r.unshift(new Buffer('1234'));
  }
});

var finished = false;
w.on('finish', function() {
  finished = true;
  // each chunk should start with 1234, and then be asfdasdfasdf...
  // The first got pulled out before the first unshift('1234'), so it's
  // lacking that piece.
  assert.equal(written[0], 'asdfasdfas');
  var asdf = 'd';
  console.error('0: %s', written[0]);
  for (var i = 1; i < written.length; i++) {
    console.error('%s: %s', i.toString(32), written[i]);
    assert.equal(written[i].slice(0, 4), '1234');
    for (var j = 4; j < written[i].length; j++) {
      var c = written[i].charAt(j);
      assert.equal(c, asdf);
      switch (asdf) {
        case 'a': asdf = 's'; break;
        case 's': asdf = 'd'; break;
        case 'd': asdf = 'f'; break;
        case 'f': asdf = 'a'; break;
      }
    }
  }
});

process.on('exit', function() {
  assert.equal(written.length, 18);
  assert(ended, 'stream ended');
  assert(finished, 'stream finished');
  console.log('ok');
});
