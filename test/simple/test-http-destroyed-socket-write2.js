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

// Verify that ECONNRESET is raised when writing to a http request
// where the server has ended the socket.

var http = require('http');
var net = require('net');
var server = http.createServer(function(req, res) {
  setImmediate(function() {
    res.destroy();
  });
});

server.listen(common.PORT, function() {
  var req = http.request({
    port: common.PORT,
    path: '/',
    method: 'POST'
  });

  var timer = setImmediate(write);
  var writes = 0;

  function write() {
    if (++writes === 128) {
      clearTimeout(timer);
      req.end();
      test();
    } else {
      timer = setImmediate(write);
      req.write('hello');
    }
  }

  var gotError = false;
  var sawData = false;
  var sawEnd = false;

  req.on('error', function(er) {
    assert(!gotError);
    gotError = true;
    switch (er.code) {
      // This is the expected case
      case 'ECONNRESET':
      // On windows this sometimes manifests as ECONNABORTED
      case 'ECONNABORTED':
        break;
      default:
        assert.strictEqual(er.code,
          'ECONNRESET',
          'Writing to a torn down client should RESET or ABORT');
        break;
    }
    clearTimeout(timer);
    console.log('ECONNRESET was raised after %d writes', writes);
    test();
  });

  req.on('response', function(res) {
    res.on('data', function(chunk) {
      console.error('saw data: ' + chunk);
      sawData = true;
    });
    res.on('end', function() {
      console.error('saw end');
      sawEnd = true;
    });
  });

  var closed = false;

  function test() {
    if (closed)
      return;

    server.close();
    closed = true;

    if (req.output.length || req.outputEncodings.length)
      console.error('bad happened', req.output, req.outputEncodings);

    assert.equal(req.output.length, 0);
    assert.equal(req.outputEncodings, 0);
    assert(gotError);
    assert(!sawData);
    assert(!sawEnd);
    console.log('ok');
  }
});
