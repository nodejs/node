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

let nchunks = 0;

const options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/'
};

const server = http.createServer(function(req, res) {
  res.writeHead(200, { 'Content-Length': '2' });
  res.write('*');
  server.once('timeout', common.mustCall(function() { res.end('*'); }));
});

server.listen(0, options.host, function() {
  options.port = this.address().port;
  const req = http.request(options, onresponse);
  req.end();

  function onresponse(res) {
    req.setTimeout(50, common.mustCall(function() {
      assert.strictEqual(nchunks, 1); // Should have received the first chunk
      server.emit('timeout');
    }));

    res.on('data', common.mustCall(function(data) {
      assert.strictEqual(String(data), '*');
      nchunks++;
    }, 2));

    res.on('end', common.mustCall(function() {
      assert.strictEqual(nchunks, 2);
      server.close();
    }));
  }
});
