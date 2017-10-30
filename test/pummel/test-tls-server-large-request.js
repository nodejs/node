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
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const stream = require('stream');
const util = require('util');

const request = Buffer.from('ABCD'.repeat(1024 * 256 - 1)); // 1mb

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

function Mediator() {
  stream.Writable.call(this);
  this.buf = '';
}
util.inherits(Mediator, stream.Writable);

Mediator.prototype._write = function _write(data, enc, cb) {
  this.buf += data;
  setTimeout(cb, 0);

  if (this.buf.length >= request.length) {
    assert.strictEqual(this.buf, request.toString());
    server.close();
  }
};

const mediator = new Mediator();

const server = tls.Server(options, common.mustCall(function(socket) {
  socket.pipe(mediator);
}));

server.listen(common.PORT, common.mustCall(function() {
  const client1 = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    client1.end(request);
  }));
}));
