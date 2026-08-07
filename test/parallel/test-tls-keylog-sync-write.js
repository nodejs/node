'use strict';

// The 'keylog' event is emitted from the TLS library's own stack, part way
// through the handshake. Writing to the socket from the handler must not
// corrupt the connection; the data must arrive intact.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');

const PAYLOAD = 'from-keylog';

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
}, common.mustCall((socket) => {
  socket.on('error', common.mustNotCall());

  const onPayload = common.mustCall(() => {
    assert.strictEqual(received, PAYLOAD);
    socket.end();
    server.close();
  });

  let received = '';
  socket.on('data', (data) => {
    received += data;
    if (received.length >= PAYLOAD.length) onPayload();
  });
}));

server.on('tlsClientError', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
  });

  // 'keylog' fires once per secret derived, so the count is version dependent.
  // Write from the first one only, to keep what the server expects exact.
  let written = false;
  client.on('keylog', common.mustCallAtLeast(() => {
    if (written) return;
    written = true;
    client.write(PAYLOAD, common.mustCall());
  }));

  client.on('error', common.mustNotCall());
}));
