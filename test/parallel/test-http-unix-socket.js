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
var fs = require('fs');
var http = require('http');

var status_ok = false; // status code == 200?
var headers_ok = false;
var body_ok = false;

var server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Type': 'text/plain',
    'Connection': 'close'
  });
  res.write('hello ');
  res.write('world\n');
  res.end();
});

server.listen(common.PIPE, function() {

  var options = {
    socketPath: common.PIPE,
    path: '/'
  };

  var req = http.get(options, function(res) {
    assert.equal(res.statusCode, 200);
    status_ok = true;

    assert.equal(res.headers['content-type'], 'text/plain');
    headers_ok = true;

    res.body = '';
    res.setEncoding('utf8');

    res.on('data', function(chunk) {
      res.body += chunk;
    });

    res.on('end', function() {
      assert.equal(res.body, 'hello world\n');
      body_ok = true;
      server.close(function(error) {
        assert.equal(error, undefined);
        server.close(function(error) {
          assert.equal(error && error.message, 'Not running');
        });
      });
    });
  });

  req.on('error', function(e) {
    console.log(e.stack);
    process.exit(1);
  });

  req.end();

});

process.on('exit', function() {
  assert.ok(status_ok);
  assert.ok(headers_ok);
  assert.ok(body_ok);
});
