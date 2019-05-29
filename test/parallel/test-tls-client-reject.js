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
  key: fixtures.readKey('rsa_private.pem'),
  cert: fixtures.readKey('rsa_cert.crt')
};

const server = tls.createServer(options, function(socket) {
  socket.pipe(socket);
  // Pipe already ends... but leaving this here tests .end() after .end().
  socket.on('end', () => socket.end());
}).listen(0, common.mustCall(function() {
  unauthorized();
}));

function unauthorized() {
  console.log('connect unauthorized');
  const socket = tls.connect({
    port: server.address().port,
    servername: 'localhost',
    rejectUnauthorized: false
  }, common.mustCall(function() {
    let _data;
    assert(!socket.authorized);
    socket.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'ok');
      _data = data;
    }));
    socket.on('end', common.mustCall(() => {
      assert(_data, 'data failed to echo!');
    }));
    socket.on('end', () => rejectUnauthorized());
  }));
  socket.once('session', common.mustCall(() => {
  }));
  socket.on('error', common.mustNotCall());
  socket.end('ok');
}

function rejectUnauthorized() {
  console.log('reject unauthorized');
  const socket = tls.connect(server.address().port, {
    servername: 'localhost'
  }, common.mustNotCall());
  socket.on('data', common.mustNotCall());
  socket.on('error', common.mustCall(function(err) {
    authorized();
  }));
  socket.end('ng');
}

function authorized() {
  console.log('connect authorized');
  const socket = tls.connect(server.address().port, {
    ca: [fixtures.readKey('rsa_cert.crt')],
    servername: 'localhost'
  }, common.mustCall(function() {
    console.log('... authorized');
    assert(socket.authorized);
    socket.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'ok');
    }));
    socket.on('end', () => server.close());
  }));
  socket.on('error', common.mustNotCall());
  socket.end('ok');
}
