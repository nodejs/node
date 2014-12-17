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

var body = 'hello world\n';

var sawFinish = false;
process.on('exit', function() {
  assert(sawFinish);
  console.log('ok');
});

var httpServer = http.createServer(function(req, res) {
  httpServer.close();

  res.on('finish', function() {
    sawFinish = true;
    assert(typeof(req.connection.bytesWritten) === 'number');
    assert(req.connection.bytesWritten > 0);
  });
  res.writeHead(200, { 'Content-Type': 'text/plain' });

  // Write 1.5mb to cause some requests to buffer
  // Also, mix up the encodings a bit.
  var chunk = new Array(1024 + 1).join('7');
  var bchunk = new Buffer(chunk);
  for (var i = 0; i < 1024; i++) {
    res.write(chunk);
    res.write(bchunk);
    res.write(chunk, 'hex');
  }
  // Get .bytesWritten while buffer is not empty
  assert(res.connection.bytesWritten > 0);

  res.end(body);
});

httpServer.listen(common.PORT, function() {
  http.get({ port: common.PORT });
});

