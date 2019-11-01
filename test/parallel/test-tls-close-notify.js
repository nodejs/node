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

// Flags: --expose-internals
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { internalBinding } = require('internal/test/binding');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { ShutdownWrap } = internalBinding('stream_wrap');

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
}, function(c) {
  // Ensure that we receive 'end' event anyway.
  c.on('end', common.mustCall(function() {
    server.close();
  }));
}).listen(0, common.mustCall(function() {
  const c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, common.mustCall(function() {
    // Send close-notify without shutting down TCP socket.
    const req = new ShutdownWrap();
    req.oncomplete = common.mustCall(() => {});
    req.handle = c._handle;
    c._handle.shutdown(req);
  }));
}));
