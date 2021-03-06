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
// Create an ssl server.  First connection, validate that not resume.
// Cache session and close connection.  Use session on second connection.
// ASSERT resumption.
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem')
};

// create server
const server = https.createServer(options, common.mustCall((req, res) => {
  res.end('Goodbye');
}, 2));

// start listening
server.listen(0, common.mustCall(function() {
  const client1 = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(() => {
    console.log('connect1');
    assert.strictEqual(client1.isSessionReused(), false);
    client1.write('GET / HTTP/1.0\r\n' +
                  'Server: 127.0.0.1\r\n' +
                  '\r\n');
  }));

  // TLS1.2 servers issue 1 ticket, TLS1.3 issues more, but only use the first.
  client1.once('session', common.mustCall((session) => {
    console.log('session');

    const opts = {
      port: server.address().port,
      rejectUnauthorized: false,
      session,
    };

    const client2 = tls.connect(opts, common.mustCall(() => {
      console.log('connect2');
      assert.strictEqual(client2.isSessionReused(), true);
      client2.write('GET / HTTP/1.0\r\n' +
                    'Server: 127.0.0.1\r\n' +
                    '\r\n');
    }));

    client2.on('close', () => {
      console.log('close2');
      server.close();
    });

    client2.resume();
  }));

  client1.resume();
}));
