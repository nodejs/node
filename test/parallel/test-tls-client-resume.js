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

// Check that the ticket from the first connection causes session resumption
// when used to make a second connection.

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

// create server
const server = tls.Server(options, common.mustCall((socket) => {
  socket.end('Goodbye');
}, 2));

// start listening
server.listen(0, common.mustCall(function() {
  let sessionx = null; // From right after connect, invalid for TLS1.3
  let session1 = null; // Delivered by the session event, always valid.
  let sessions = 0;
  let tls13;
  const client1 = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(() => {
    tls13 = client1.getProtocol() === 'TLSv1.3';
    assert.strictEqual(client1.isSessionReused(), false);
    sessionx = client1.getSession();
    assert(sessionx);

    if (session1)
      reconnect();
  }));

  client1.on('data', common.mustCall());

  client1.once('session', common.mustCall((session) => {
    console.log('session1');
    session1 = session;
    assert(session1);
    if (sessionx)
      reconnect();
  }));

  client1.on('session', () => {
    console.log('client1 session#', ++sessions);
  });

  client1.on('close', () => {
    console.log('client1 close');
    assert.strictEqual(sessions, tls13 ? 2 : 1);
  });

  function reconnect() {
    assert(sessionx);
    assert(session1);
    if (tls13)
      // For TLS1.3, the session immediately after handshake is a dummy,
      // unresumable session. The one delivered later in session event is
      // resumable.
      assert.notStrictEqual(sessionx.compare(session1), 0);
    else
      // For TLS1.2, they are identical.
      assert.strictEqual(sessionx.compare(session1), 0);

    const opts = {
      port: server.address().port,
      rejectUnauthorized: false,
      session: session1,
    };

    const client2 = tls.connect(opts, common.mustCall(() => {
      console.log('connect2');
      assert.strictEqual(client2.isSessionReused(), true);
    }));

    client2.on('close', common.mustCall(() => {
      console.log('close2');
      server.close();
    }));

    client2.resume();
  }

  client1.resume();
}));
