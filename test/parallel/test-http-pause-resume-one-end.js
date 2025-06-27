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
const http = require('http');
const assert = require('assert');

const server = http.Server(function(req, res) {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
  server.close();
});

server.listen(0, common.mustCall(function() {
  const opts = {
    port: this.address().port,
    headers: { connection: 'close' }
  };

  http.get(opts, common.mustCall(function(res) {
    res.on('data', common.mustCall(function() {
      res.pause();
      setImmediate(function() {
        res.resume();
      });
    }));

    res.on('end', common.mustCall(() => {
      assert.strictEqual(res.destroyed, false);
    }));
    assert.strictEqual(res.destroyed, false);
    res.on('close', common.mustCall(() => {
      assert.strictEqual(res.destroyed, true);
    }));
  }));
}));
