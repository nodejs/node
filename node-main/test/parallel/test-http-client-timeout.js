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
require('../common');
const assert = require('assert');
const http = require('http');

const options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/'
};

const server = http.createServer(function(req, res) {
  // This space intentionally left blank
});

server.listen(0, options.host, function() {
  options.port = this.address().port;
  const req = http.request(options, function(res) {
    // This space intentionally left blank
  });
  req.on('close', function() {
    assert.strictEqual(req.destroyed, true);
    server.close();
  });
  function destroy() {
    req.destroy();
  }
  const s = req.setTimeout(1, destroy);
  assert.ok(s instanceof http.ClientRequest);
  req.on('error', destroy);
  req.end();
});
