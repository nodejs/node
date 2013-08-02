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

var common = require('../common.js');
var stream = require('../../readable');
var Readable = stream.Readable;
var Writable = stream.Writable;
var assert = require('assert');

var util = require('util');
var EE = require('events').EventEmitter;


// a mock thing a bit like the net.Socket/tcp_wrap.handle interaction

var stream = new Readable({
  highWaterMark: 16,
  encoding: 'utf8'
});

var source = new EE;

stream._read = function() {
  console.error('stream._read');
  readStart();
};

var ended = false;
stream.on('end', function() {
  ended = true;
});

source.on('data', function(chunk) {
  var ret = stream.push(chunk);
  console.error('data', stream._readableState.length);
  if (!ret)
    readStop();
});

source.on('end', function() {
  stream.push(null);
});

var reading = false;

function readStart() {
  console.error('readStart');
  reading = true;
}

function readStop() {
  console.error('readStop');
  reading = false;
  process.nextTick(function() {
    var r = stream.read();
    if (r !== null)
      writer.write(r);
  });
}

var writer = new Writable({
  decodeStrings: false
});

var written = [];

var expectWritten =
  [ 'asdfgasdfgasdfgasdfg',
    'asdfgasdfgasdfgasdfg',
    'asdfgasdfgasdfgasdfg',
    'asdfgasdfgasdfgasdfg',
    'asdfgasdfgasdfgasdfg',
    'asdfgasdfgasdfgasdfg' ];

writer._write = function(chunk, encoding, cb) {
  console.error('WRITE %s', chunk);
  written.push(chunk);
  process.nextTick(cb);
};

writer.on('finish', finish);


// now emit some chunks.

var chunk = "asdfg";

var set = 0;
readStart();
data();
function data() {
  assert(reading);
  source.emit('data', chunk);
  assert(reading);
  source.emit('data', chunk);
  assert(reading);
  source.emit('data', chunk);
  assert(reading);
  source.emit('data', chunk);
  assert(!reading);
  if (set++ < 5)
    setTimeout(data, 10);
  else
    end();
}

function finish() {
  console.error('finish');
  assert.deepEqual(written, expectWritten);
  console.log('ok');
}

function end() {
  source.emit('end');
  assert(!reading);
  writer.end(stream.read());
  setTimeout(function() {
    assert(ended);
  });
}
