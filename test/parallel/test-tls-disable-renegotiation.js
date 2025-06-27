'use strict';
const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

// Tests that calling disableRenegotiation on a TLSSocket stops renegotiation.

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

// Renegotiation as a protocol feature was dropped after TLS1.2.
tls.DEFAULT_MAX_VERSION = 'TLSv1.2';

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};

const server = tls.Server(options, common.mustCall((socket) => {
  socket.on('error', common.mustCall((err) => {
    common.expectsError({
      name: 'Error',
      code: 'ERR_TLS_RENEGOTIATION_DISABLED',
      message: 'TLS session renegotiation disabled for this socket'
    })(err);
    socket.destroy();
    server.close();
  }));
  // Disable renegotiation after the first chunk of data received.
  // Demonstrates that renegotiation works successfully up until
  // disableRenegotiation is called.
  socket.on('data', common.mustCall((chunk) => {
    socket.write(chunk);
    socket.disableRenegotiation();
  }));
  socket.on('secure', common.mustCall(() => {
    assert(socket._handle.handshakes < 2,
           `Too many handshakes [${socket._handle.handshakes}]`);
  }));
}));


server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const options = {
    rejectUnauthorized: false,
    port
  };
  const client = tls.connect(options, common.mustCall(() => {

    assert.throws(() => client.renegotiate(), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });

    assert.throws(() => client.renegotiate(common.mustNotCall()), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });

    assert.throws(() => client.renegotiate({}, false), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });

    assert.throws(() => client.renegotiate({}, null), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });


    // Negotiation is still permitted for this first
    // attempt. This should succeed.
    let ok = client.renegotiate(options, common.mustSucceed(() => {
      // Once renegotiation completes, we write some
      // data to the socket, which triggers the on
      // data event on the server. After that data
      // is received, disableRenegotiation is called.
      client.write('data', common.mustCall(() => {
        // This second renegotiation attempt should fail
        // and the callback should never be invoked. The
        // server will simply drop the connection after
        // emitting the error.
        ok = client.renegotiate(options, common.mustNotCall());
        assert.strictEqual(ok, true);
      }));
    }));
    assert.strictEqual(ok, true);
    client.on('secureConnect', common.mustCall());
    client.on('secure', common.mustCall());
  }));
}));
