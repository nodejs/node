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

'use strict';
require('../common');
const assert = require('assert');

const http = require('http');

const Stream = require('stream');

function getSrc() {
  // An old-style readable stream.
  // The Readable class prevents this behavior.
  const src = new Stream();

  // start out paused, just so we don't miss anything yet.
  let paused = false;
  src.pause = function() {
    paused = true;
  };
  src.resume = function() {
    paused = false;
  };

  const chunks = [ '', 'asdf', '', 'foo', '', 'bar', '' ];
  const interval = setInterval(function() {
    if (paused)
      return;

    const chunk = chunks.shift();
    if (chunk !== undefined) {
      src.emit('data', chunk);
    } else {
      src.emit('end');
      clearInterval(interval);
    }
  }, 1);

  return src;
}


const expect = 'asdffoobar';

const server = http.createServer(function(req, res) {
  let actual = '';
  req.setEncoding('utf8');
  req.on('data', function(c) {
    actual += c;
  });
  req.on('end', function() {
    assert.strictEqual(actual, expect);
    getSrc().pipe(res);
  });
  server.close();
});

server.listen(0, function() {
  const req = http.request({ port: this.address().port, method: 'POST' });
  let actual = '';
  req.on('response', function(res) {
    res.setEncoding('utf8');
    res.on('data', function(c) {
      actual += c;
    });
    res.on('end', function() {
      assert.strictEqual(actual, expect);
    });
  });
  getSrc().pipe(req);
});

process.on('exit', function(c) {
  if (!c) console.log('ok');
});
