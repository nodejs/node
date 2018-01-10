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

const bufSize = 1024 * 1024;
let sent = 0;
let received = 0;

const server = tls.Server(options, function(socket) {
  socket.pipe(socket);
  socket.on('data', function(c) {
    console.error('data', c.length);
  });
});

server.listen(0, function() {
  let resumed = false;
  const client = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, function() {
    console.error('connected');
    client.pause();
    console.error('paused');
    send();
    function send() {
      console.error('sending');
      const ret = client.write(Buffer.allocUnsafe(bufSize));
      console.error(`write => ${ret}`);
      if (false !== ret) {
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
    }
  });
  client.on('data', function(data) {
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
});

process.on('exit', function() {
  assert.strictEqual(sent, received);
});
