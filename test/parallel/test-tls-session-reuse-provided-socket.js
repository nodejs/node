// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const { hasMultiLocalhost } = require('../common/net');
const net = require('net');
const tls = require('tls');
const { once } = require('events');

async function connectWithSession(
  socket, session, maxVersion, expectedReuse = true, servername,
) {
  const tlsSocket = tls.connect({
    socket,
    session,
    servername,
    maxVersion,
    rejectUnauthorized: false,
  });
  const close = once(tlsSocket, 'close');
  tlsSocket.resume();
  await once(tlsSocket, 'secureConnect');
  assert.strictEqual(tlsSocket.isSessionReused(), expectedReuse);
  await close;
}

async function connectAndCaptureSession(
  port, maxVersion, host = '127.0.0.1', lookup,
) {
  const socket = tls.connect({
    host,
    lookup,
    port,
    maxVersion,
    rejectUnauthorized: false,
  });
  const close = once(socket, 'close');
  const sessionPromise = once(socket, 'session').then(([session]) => session);
  socket.resume();
  await once(socket, 'secureConnect');
  assert.strictEqual(socket.isSessionReused(), false);
  const session = await sessionPromise;
  assert(Buffer.isBuffer(session));
  await close;
  return session;
}

async function testVersion(maxVersion) {
  const server = tls.createServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
    minVersion: maxVersion,
    maxVersion,
  }, (socket) => socket.end('ok'));

  server.listen(0, '0.0.0.0');
  await once(server, 'listening');
  const { port } = server.address();
  const session = await connectAndCaptureSession(port, maxVersion);

  let socket = net.connect({ host: '127.0.0.1', port });
  await once(socket, 'connect');
  await connectWithSession(socket, session, maxVersion);

  socket = net.connect({ host: '127.0.0.1', port });
  await connectWithSession(socket, session, maxVersion);

  if (hasMultiLocalhost()) {
    socket = net.connect({ host: '127.0.0.2', port });
    await connectWithSession(socket, session, maxVersion, false);
  }

  const lookup = (_hostname, _options, callback) => {
    if (_options.all) {
      callback(null, [{ address: '127.0.0.1', family: 4 }]);
    } else {
      callback(null, '127.0.0.1', 4);
    }
  };
  const hostnameSession = await connectAndCaptureSession(
    port, maxVersion, 'localhost', lookup);
  socket = net.connect({ host: 'localhost', port, lookup });
  await connectWithSession(socket, hostnameSession, maxVersion);

  socket = net.connect({ host: '127.0.0.1', port });
  await connectWithSession(socket, session, maxVersion, false, 'localhost');

  server.close();
  await once(server, 'close');
}

(async function() {
  await testVersion('TLSv1.2');
  await testVersion('TLSv1.3');
})().then(common.mustCall());
