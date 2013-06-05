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

// This test verifies that stream.unshift(Buffer(0)) or 
// stream.unshift('') does not set state.reading=false.
var Readable = require('stream').Readable;

var r = new Readable();
var nChunks = 10;
var chunk = new Buffer(10);
chunk.fill('x');

r._read = function(n) {
  setTimeout(function() {
    r.push(--nChunks === 0 ? null : chunk);
  });
};

var readAll = false;
var seen = [];
r.on('readable', function() {
  var chunk;
  while (chunk = r.read()) {
    seen.push(chunk.toString());
    // simulate only reading a certain amount of the data,
    // and then putting the rest of the chunk back into the
    // stream, like a parser might do.  We just fill it with
    // 'y' so that it's easy to see which bits were touched,
    // and which were not.
    var putBack = new Buffer(readAll ? 0 : 5);
    putBack.fill('y');
    readAll = !readAll;
    r.unshift(putBack);
  }
});

var expect =
  [ 'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy' ];

r.on('end', function() {
  assert.deepEqual(seen, expect);
  console.log('ok');
});
