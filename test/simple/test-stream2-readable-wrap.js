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

var Readable = require('_stream_readable');
var Writable = require('_stream_writable');
var EE = require('events').EventEmitter;

var old = new EE;
var r = new Readable({ highWaterMark: 10 });
assert.equal(r, r.wrap(old));

var ended = false;
r.on('end', function() {
  ended = true;
});

var pauses = 0;
var resumes = 0;

old.pause = function() {
  pauses++;
  old.emit('pause');
  flowing = false;
};

old.resume = function() {
  resumes++;
  old.emit('resume');
  flow();
};

var flowing;
var chunks = 10;
var oldEnded = false;
function flow() {
  flowing = true;
  while (flowing && chunks-- > 0) {
    old.emit('data', new Buffer('xxxxxxxxxx'));
  }
  if (chunks <= 0) {
    oldEnded = true;
    old.emit('end');
  }
}

var w = new Writable({ highWaterMark: 20 });
var written = [];
w._write = function(chunk, encoding, cb) {
  written.push(chunk.toString());
  setTimeout(cb);
};

var finished = false;
w.on('finish', function() {
  finished = true;
});


var expect = new Array(11).join('xxxxxxxxxx');

r.pipe(w);

flow();

process.on('exit', function() {
  assert.equal(pauses, 10);
  assert.equal(resumes, 9);
  assert(ended);
  assert(finished);
  assert(oldEnded);
  assert.equal(written.join(''), expect);
  console.log('ok');
});
