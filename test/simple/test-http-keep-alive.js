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
var headers = {'connection': 'keep-alive'};

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Length': body.length});
  res.write(body);
  res.end();
});

var connectCount = 0;

server.listen(common.PORT, function() {
  var client = http.createClient(common.PORT);

  client.addListener('connect', function() {
    common.error('CONNECTED');
    connectCount++;
  });

  var request = client.request('GET', '/', headers);
  request.end();
  request.addListener('response', function(response) {
    common.error('response start');

    response.addListener('end', function() {
      common.error('response end');
      var req = client.request('GET', '/', headers);
      req.addListener('response', function(response) {
        response.addListener('end', function() {
          client.end();
          server.close();
        });
      });
      req.end();
    });
  });
});

process.addListener('exit', function() {
  assert.equal(1, connectCount);
});
