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

var expectedHeaders = {
  'DELETE': ['host', 'connection'],
  'GET': ['host', 'connection'],
  'HEAD': ['host', 'connection'],
  'OPTIONS': ['host', 'connection'],
  'POST': ['host', 'connection', 'transfer-encoding'],
  'PUT': ['host', 'connection', 'transfer-encoding']
};

var expectedMethods = Object.keys(expectedHeaders);

var requestCount = 0;

var server = http.createServer(function(req, res) {
  requestCount++;
  res.end();

  assert(expectedHeaders.hasOwnProperty(req.method),
         req.method + ' was an unexpected method');

  var requestHeaders = Object.keys(req.headers);
  requestHeaders.forEach(function(header) {
    assert(expectedHeaders[req.method].indexOf(header.toLowerCase()) !== -1,
           header + ' shoud not exist for method ' + req.method);
  });

  assert(requestHeaders.length === expectedHeaders[req.method].length,
         'some headers were missing for method: ' + req.method);

  if (expectedMethods.length === requestCount)
    server.close();
});

server.listen(common.PORT, function() {
  expectedMethods.forEach(function(method) {
    http.request({
      method: method,
      port: common.PORT
    }).end();
  });
});
