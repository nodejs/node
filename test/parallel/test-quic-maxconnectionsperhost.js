// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { createSocket } = require('quic');
const assert = require('assert');
const Countdown = require('../common/countdown');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const kServerName = 'agent2';
const kALPN = 'zzz';

// QuicSockets must throw errors when maxConnectionsPerHost is not a
// safe integer or is out of range.
{
  [-1, 0].forEach((maxConnectionsPerHost) => {
    assert.throws(() => createSocket({ maxConnectionsPerHost }), {
      code: 'ERR_OUT_OF_RANGE'
    });
  });

  [Number.MAX_SAFE_INTEGER + 1, 1.1].forEach((maxConnectionsPerHost) => {
    assert.throws(() => createSocket({ maxConnectionsPerHost }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });
}

// Test that new client sessions will be closed when it exceeds
// maxConnectionsPerHost.
{
  const kMaxConnectionsPerHost = 5;
  const kIdleTimeout = 0;

  let client;
  let server;

  const countdown = new Countdown(kMaxConnectionsPerHost + 1, () => {
    client.close();
    server.close();
  });

  function connect() {
    return client.connect({
      key,
      cert,
      ca,
      address: common.localhostIPv4,
      port: server.endpoints[0].address.port,
      servername: kServerName,
      alpn: kALPN,
      idleTimeout: kIdleTimeout,
    });
  }

  server = createSocket({ maxConnectionsPerHost: kMaxConnectionsPerHost });

  server.listen({ key, cert, ca, alpn: kALPN, idleTimeout: kIdleTimeout });

  server.on('session', common.mustCall(() => {}, kMaxConnectionsPerHost));

  server.on('close', common.mustCall(() => {
    assert.strictEqual(server.serverBusyCount, 1n);
  }));

  server.on('ready', common.mustCall(() => {
    client = createSocket();

    const sessions = [];

    for (let i = 0; i < kMaxConnectionsPerHost; i += 1) {
      const req = connect();
      req.on('error', common.mustNotCall());
      req.on('close', common.mustCall(() => countdown.dec()));
      sessions.push(req);
    }

    const extra = connect();
    extra.on('error', console.log);
    extra.on('close', common.mustCall(() => {
      countdown.dec();
      // Shutdown the remaining open sessions.
      setImmediate(common.mustCall(() => {
        for (const req of sessions)
          req.close();
      }));
    }));

  }));
}
