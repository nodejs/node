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

const options = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem')
};

// Contains a UTF8 only character
const messageUtf8 = 'x√ab c';

// The same string above represented with ASCII
const messageAscii = 'xb\b\u001aab c';

const server = tls.Server(options, common.mustCall(function(socket) {
  socket.end(messageUtf8);
}));


server.listen(0, function() {
  const client = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  });

  let buffer = '';

  client.setEncoding('ascii');

  client.on('data', function(d) {
    assert.ok(typeof d === 'string');
    buffer += d;
  });


  client.on('close', function() {
    // readyState is deprecated but we want to make
    // sure this isn't triggering an assert in lib/net.js
    // See issue #1069.
    assert.strictEqual('closed', client.readyState);

    // Confirming the buffer string is encoded in ASCII
    // and thus does NOT match the UTF8 string
    assert.notStrictEqual(buffer, messageUtf8);

    // Confirming the buffer string is encoded in ASCII
    // and thus does equal the ASCII string representation
    assert.strictEqual(buffer, messageAscii);

    server.close();
  });
});
