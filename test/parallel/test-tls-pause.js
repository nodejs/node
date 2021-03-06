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

// This test ensures that the data received over tls-server after pause
// is same as what it was sent

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('rsa_private.pem'),
  cert: fixtures.readKey('rsa_cert.crt')
};

const bufSize = 1024 * 1024;
let sent = 0;
let received = 0;

const server = tls.Server(options, common.mustCall((socket) => {
  socket.pipe(socket);
  socket.on('data', (c) => {
    console.error('data', c.length);
  });
}));

server.listen(0, common.mustCall(() => {
  let resumed = false;
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false
  }, common.mustCall(() => {
    console.error('connected');
    client.pause();
    console.error('paused');
    const send = (() => {
      console.error('sending');
      const ret = client.write(Buffer.allocUnsafe(bufSize));
      console.error(`write => ${ret}`);
      if (ret !== false) {
        console.error('write again');
        sent += bufSize;
        assert.ok(sent < 100 * 1024 * 1024); // max 100MB
        return process.nextTick(send);
      }
      sent += bufSize;
      console.error(`sent: ${sent}`);
      resumed = true;
      client.resume();
      console.error('resumed', client);
    })();
  }));
  client.on('data', (data) => {
    console.error('data');
    assert.ok(resumed);
    received += data.length;
    console.error('received', received);
    console.error('sent', sent);
    if (received >= sent) {
      console.error(`received: ${received}`);
      client.end();
      server.close();
    }
  });
}));

process.on('exit', () => {
  assert.strictEqual(sent, received);
});
