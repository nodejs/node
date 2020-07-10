// Flags: --expose-internals --no-warnings
'use strict';

// Tests QUIC keylogging

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { key, cert, ca } = require('../common/quic');
const { once } = require('events');

const { createQuicSocket } = require('net');

const kKeylogs = [
  /^CLIENT_HANDSHAKE_TRAFFIC_SECRET .*/,
  /^SERVER_HANDSHAKE_TRAFFIC_SECRET .*/,
  /^QUIC_CLIENT_HANDSHAKE_TRAFFIC_SECRET .*/,
  /^QUIC_SERVER_HANDSHAKE_TRAFFIC_SECRET .*/,
  /^CLIENT_TRAFFIC_SECRET_0 .*/,
  /^SERVER_TRAFFIC_SECRET_0 .*/,
  /^QUIC_CLIENT_TRAFFIC_SECRET_0 .*/,
  /^QUIC_SERVER_TRAFFIC_SECRET_0 .*/,
];

const options = { key, cert, ca, alpn: 'zzz' };

const server = createQuicSocket({ server: options });
const client = createQuicSocket({ client: options });

const kServerKeylogs = Array.from(kKeylogs);
const kClientKeylogs = Array.from(kKeylogs);

(async () => {

  server.on('session', common.mustCall((session) => {
    session.on('keylog', common.mustCall((line) => {
      assert.match(line.toString(), kServerKeylogs.shift());
    }, kServerKeylogs.length));
  }));

  await server.listen();

  const req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });

  req.on('keylog', common.mustCall((line) => {
    assert.match(line.toString(), kClientKeylogs.shift());
  }, kClientKeylogs.length));

  await once(req, 'secure');

  server.close();
  client.close();

  await Promise.allSettled([
    once(server, 'close'),
    once(client, 'close')
  ]);

  assert.strictEqual(kServerKeylogs.length, 0);
  assert.strictEqual(kClientKeylogs.length, 0);
})().then(common.mustCall());
