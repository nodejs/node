// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { createQuicSocket } = require('net');
const assert = require('assert');
const Countdown = require('../common/countdown');
const { key, cert, ca } = require('../common/quic');
const options = { key, cert, ca, alpn: 'zzz', idleTimeout: 0 };

// QuicSockets must throw errors when maxConnectionsPerHost is not a
// safe integer or is out of range.
{
  [-1, 0, Number.MAX_SAFE_INTEGER + 1, 1.1].forEach((maxConnectionsPerHost) => {
    assert.throws(() => createQuicSocket({ maxConnectionsPerHost }), {
      code: 'ERR_OUT_OF_RANGE'
    });
  });
}

// Test that new client sessions will be closed when it exceeds
// maxConnectionsPerHost.
(async function() {
  const kMaxConnectionsPerHost = 5;

  const client = createQuicSocket({ client: options });
  const server = createQuicSocket({
    maxConnectionsPerHost: kMaxConnectionsPerHost,
    server: options
  });

  const countdown = new Countdown(kMaxConnectionsPerHost + 1, () => {
    client.close();
    server.close();
  });

  server.on('session', common.mustCall(() => {}, kMaxConnectionsPerHost));

  server.on('close', common.mustCall(() => {
    assert.strictEqual(server.serverBusyCount, 1);
  }));

  await server.listen();

  const sessions = [];
  for (let i = 0; i < kMaxConnectionsPerHost; i += 1) {
    const req = await client.connect({
      address: common.localhostIPv4,
      port: server.endpoints[0].address.port,
    });
    req.on('error', common.mustNotCall());
    req.on('close', common.mustCall(() => countdown.dec()));
    sessions.push(req);
  }

  const extra = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });
  extra.on('error', common.mustNotCall());
  extra.on('close', common.mustCall(() => {
    assert.strictEqual(extra.closeCode.code, 2);
    countdown.dec();
    // Shutdown the remaining open sessions.
    setImmediate(common.mustCall(() => {
      for (const req of sessions)
        req.close();
    }));
  }));
})().then(common.mustCall());
