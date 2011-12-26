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

var expectedServer = 'Request Body from Client';
var resultServer = '';
var expectedClient = 'Response Body from Server';
var resultClient = '';

var server = http.createServer(function(req, res) {
  common.debug('pause server request');
  req.pause();
  setTimeout(function() {
    common.debug('resume server request');
    req.resume();
    req.setEncoding('utf8');
    req.on('data', function(chunk) {
      resultServer += chunk;
    });
    req.on('end', function() {
      common.debug(resultServer);
      res.writeHead(200);
      res.end(expectedClient);
    });
  }, 100);
});

server.listen(common.PORT, function() {
  var req = http.request({
    port: common.PORT,
    path: '/',
    method: 'POST'
  }, function(res) {
    common.debug('pause client response');
    res.pause();
    setTimeout(function() {
      common.debug('resume client response');
      res.resume();
      res.on('data', function(chunk) {
        resultClient += chunk;
      });
      res.on('end', function() {
        common.debug(resultClient);
        server.close();
      });
    }, 100);
  });
  req.end(expectedServer);
});

process.on('exit', function() {
  assert.equal(expectedServer, resultServer);
  assert.equal(expectedClient, resultClient);
});
