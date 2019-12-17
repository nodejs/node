'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { createSocket } = require('quic');
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
    common.expectsError(() => createSocket({
      endpoint: { port: 0 },
      maxConnectionsPerHost
    }), {
      type: RangeError,
      code: 'ERR_OUT_OF_RANGE',
      message: /The value of "options\.maxConnectionsPerHost" is out of range/
    });
  });

  [Number.MAX_SAFE_INTEGER + 1, 1.1].forEach((maxConnectionsPerHost) => {
    common.expectsError(() => createSocket({
      endpoint: { port: 0 },
      maxConnectionsPerHost
    }), {
      type: TypeError,
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.maxConnectionsPerHost" property must be of type safe integer\. Received type/
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

  function connect() {
    return client.connect({
      key,
      cert,
      ca,
      address: common.localhostIPv4,
      port: server.address.port,
      servername: kServerName,
      alpn: kALPN,
      idleTimeout: kIdleTimeout,
    });
  }

  server = createSocket({
    endpoint: { port: 0 },
    maxConnectionsPerHost: kMaxConnectionsPerHost,
  });

  server.listen({
    key,
    cert,
    ca,
    alpn: kALPN,
    idleTimeout: kIdleTimeout,
  });

  server.on('session', common.mustCall(() => {
    // TODO(@oyyd): When maxConnectionsPerHost is exceeded, the new session
    // will still be emitted and won't be destroied automatically. We need
    // to figure out the reason and fix it.
  }, kMaxConnectionsPerHost + 1));

  server.on('ready', common.mustCall(async () => {
    client = createSocket({ endpoint: { port: 0 } });

    const sessions = [];

    for (let i = 0; i < kMaxConnectionsPerHost; i += 1) {
      const session = await new Promise((resolve) => {
        const clientSession = connect();

        clientSession.on('error', common.mustNotCall());
        clientSession.on('secure', common.mustCall(() => {
          resolve(clientSession);
        }));
      });

      sessions.push(session);
    }

    // Sessions will be closed if the number of connceted sessions is equal
    // to maxConnectionsPerHost.
    await new Promise((resolve) => {
      const clientSession = connect();
      clientSession.on('error', common.mustNotCall());
      clientSession.on('close', common.mustCall(() => {
        resolve();
      }));
    });

    client.close();
    server.close();
  }));
}
