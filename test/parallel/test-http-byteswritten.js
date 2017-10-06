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
const common = require('../common');
const assert = require('assert');
const http = require('http');

const body = 'hello world\n';

const httpServer = http.createServer(common.mustCall(function(req, res) {
  httpServer.close();

  res.on('finish', common.mustCall(function() {
    assert.strictEqual(typeof req.connection.bytesWritten, 'number');
    assert(req.connection.bytesWritten > 0);
  }));
  res.writeHead(200, { 'Content-Type': 'text/plain' });

  // Write 1.5mb to cause some requests to buffer
  // Also, mix up the encodings a bit.
  const chunk = '7'.repeat(1024);
  const bchunk = Buffer.from(chunk);
  for (let i = 0; i < 1024; i++) {
    res.write(chunk);
    res.write(bchunk);
    res.write(chunk, 'hex');
  }
  // Get .bytesWritten while buffer is not empty
  assert(res.connection.bytesWritten > 0);

  res.end(body);
}));

httpServer.listen(0, function() {
  http.get({ port: this.address().port });
});
