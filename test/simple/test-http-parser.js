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

// The purpose of this test is not to check HTTP compliance but to test the
// binding. Tests for pathological http messages should be submitted
// upstream to http://github.com/ry/http-parser for inclusion into
// deps/http-parser/test.c

var HTTPParser = process.binding('http_parser').HTTPParser;

var parser = new HTTPParser('request');

var Buffer = require('buffer').Buffer;
var buffer = new Buffer(1024);

var request = 'GET /hello HTTP/1.1\r\n\r\n';

buffer.write(request, 0, 'ascii');

var callbacks = 0;

parser.onMessageBegin = function() {
  console.log('message begin');
  callbacks++;
};

parser.onHeadersComplete = function(info) {
  console.log('headers complete: ' + JSON.stringify(info));
  assert.equal('GET', info.method);
  assert.equal(1, info.versionMajor);
  assert.equal(1, info.versionMinor);
  callbacks++;
};

parser.onURL = function(b, off, len) {
  //throw new Error('hello world');
  callbacks++;
};

parser.onPath = function(b, off, length) {
  console.log('path [' + off + ', ' + length + ']');
  var path = b.toString('ascii', off, off + length);
  console.log('path = "' + path + '"');
  assert.equal('/hello', path);
  callbacks++;
};

parser.execute(buffer, 0, request.length);
assert.equal(4, callbacks);

//
// Check that if we throw an error in the callbacks that error will be
// thrown from parser.execute()
//

parser.onURL = function(b, off, len) {
  throw new Error('hello world');
};

assert.throws(function() {
  parser.execute(buffer, 0, request.length);
}, Error, 'hello world');

