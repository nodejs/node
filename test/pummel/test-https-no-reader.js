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

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const Buffer = require('buffer').Buffer;
const fs = require('fs');
const path = require('path');

const options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

const buf = Buffer.allocUnsafe(1024 * 1024);

const server = https.createServer(options, function(req, res) {
  res.writeHead(200);
  for (let i = 0; i < 50; i++) {
    res.write(buf);
  }
  res.end();
});

server.listen(common.PORT, function() {
  const req = https.request({
    method: 'POST',
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    res.read(0);

    setTimeout(function() {
      // Read buffer should be somewhere near high watermark
      // (i.e. should not leak)
      assert(res._readableState.length < 100 * 1024);
      process.exit(0);
    }, 2000);
  });
  req.end();
});
