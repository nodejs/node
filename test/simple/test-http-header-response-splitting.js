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

var common = require('../common'),
    assert = require('assert'),
    http = require('http');

var testIndex = 0,
    responses = 0;

var server = http.createServer(function(req, res) {
  switch (testIndex++) {
    case 0:
      res.writeHead(200, { test: 'foo \r\ninvalid: bar' });
      break;
    case 1:
      res.writeHead(200, { test: 'foo \ninvalid: bar' });
      break;
    case 2:
      res.writeHead(200, { test: 'foo \rinvalid: bar' });
      break;
    case 3:
      res.writeHead(200, { test: 'foo \n\n\ninvalid: bar' });
      break;
    case 4:
      res.writeHead(200, { test: 'foo \r\n \r\n \r\ninvalid: bar' });
      server.close();
      break;
    default:
      assert(false);
  }
  res.end('Hi mars!');
});

server.listen(common.PORT, function() {
  for (var i = 0; i < 5; i++) {
    var req = http.get({ port: common.PORT, path: '/' }, function(res) {
      assert.strictEqual(res.headers.test, 'foo invalid: bar');
      assert.strictEqual(res.headers.invalid, undefined);
      responses++;
      res.resume();
    });
  }
});

process.on('exit', function() {
  assert.strictEqual(responses, 5);
});
