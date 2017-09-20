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
  key: fixtures.readSync('test_key.pem'),
  cert: fixtures.readSync('test_cert.pem')
};

const server = tls.createServer(options, common.mustCall(function(socket) {
  socket.on('data', function(data) {
    console.error(data.toString());
    assert.strictEqual(data.toString(), 'ok');
  });
}, 3)).listen(0, function() {
  unauthorized();
});

function unauthorized() {
  const socket = tls.connect({
    port: server.address().port,
    servername: 'localhost',
    rejectUnauthorized: false
  }, common.mustCall(function() {
    assert(!socket.authorized);
    socket.end();
    rejectUnauthorized();
  }));
  socket.on('error', common.mustNotCall());
  socket.write('ok');
}

function rejectUnauthorized() {
  const socket = tls.connect(server.address().port, {
    servername: 'localhost'
  }, common.mustNotCall());
  socket.on('error', common.mustCall(function(err) {
    console.error(err);
    authorized();
  }));
  socket.write('ng');
}

function authorized() {
  const socket = tls.connect(server.address().port, {
    ca: [fixtures.readSync('test_cert.pem')],
    servername: 'localhost'
  }, common.mustCall(function() {
    assert(socket.authorized);
    socket.end();
    server.close();
  }));
  socket.on('error', common.mustNotCall());
  socket.write('ok');
}
