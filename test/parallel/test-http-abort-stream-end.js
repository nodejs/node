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

const maxSize = 1024;
let size = 0;

const server = http.createServer(common.mustCall((req, res) => {
  server.close();

  res.writeHead(200, {'Content-Type': 'text/plain'});
  for (let i = 0; i < maxSize; i++) {
    res.write(`x${i}`);
  }
  res.end();
}));

let aborted = false;
server.listen(0, () => {

  const res = common.mustCall((res) => {
    res.on('data', (chunk) => {
      size += chunk.length;
      assert(!aborted, 'got data after abort');
      if (size > maxSize) {
        aborted = true;
        req.abort();
        size = maxSize;
      }
    });

    req.on('abort', common.mustCall(() => assert.strictEqual(size, maxSize)));
  });

  const req = http.get(`http://localhost:${server.address().port}`, res);
});
