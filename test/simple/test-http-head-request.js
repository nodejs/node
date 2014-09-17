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
var util = require('util');


var body = 'hello world\n';
var id = 0;

function test(headers) {
  var port = common.PORT + id++;

  var server = http.createServer(function(req, res) {
    console.error('req: %s headers: %j', req.method, headers);
    res.writeHead(200, headers);
    res.end();
    server.close();
  });

  var gotEnd = false;

  server.listen(port, function() {
    var request = http.request({
      port: port,
      method: 'HEAD',
      path: '/'
    }, function(response) {
      console.error('response start');
      response.on('end', function() {
        console.error('response end');
        gotEnd = true;
      });
      response.resume();
    });
    request.end();
  });

  process.on('exit', function() {
    assert.ok(gotEnd);
  });
}

test({
  'Transfer-Encoding': 'chunked'
});
test({
  'Content-Length': body.length
});
