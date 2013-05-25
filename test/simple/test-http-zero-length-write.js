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

var http = require('http');

var Stream = require('stream');

function getSrc() {
  // An old-style readable stream.
  // The Readable class prevents this behavior.
  var src = new Stream();

  // start out paused, just so we don't miss anything yet.
  var paused = false;
  src.pause = function() {
    paused = true;
  };
  src.resume = function() {
    paused = false;
  };

  var chunks = [ '', 'asdf', '', 'foo', '', 'bar', '' ];
  var interval = setInterval(function() {
    if (paused)
      return

    var chunk = chunks.shift();
    if (chunk !== undefined) {
      src.emit('data', chunk);
    } else {
      src.emit('end');
      clearInterval(interval);
    }
  }, 1);

  return src;
}


var expect = 'asdffoobar';

var server = http.createServer(function(req, res) {
  var actual = '';
  req.setEncoding('utf8');
  req.on('data', function(c) {
    actual += c;
  });
  req.on('end', function() {
    assert.equal(actual, expect);
    getSrc().pipe(res);
  });
  server.close();
});

server.listen(common.PORT, function() {
  var req = http.request({ port: common.PORT, method: 'POST' });
  var actual = '';
  req.on('response', function(res) {
    res.setEncoding('utf8');
    res.on('data', function(c) {
      actual += c;
    });
    res.on('end', function() {
      assert.equal(actual, expect);
    });
  });
  getSrc().pipe(req);
});

process.on('exit', function(c) {
  if (!c) console.log('ok');
});
