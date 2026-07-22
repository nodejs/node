'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');
const { once } = require('events');

function load(name) {
  return fixtures.readKey(name);
}

const contexts = {
  agent1: tls.createSecureContext({
    key: load('agent1-key.pem'),
    cert: load('agent1-cert.pem'),
  }),
  agent3: tls.createSecureContext({
    key: load('agent3-key.pem'),
    cert: load('agent3-cert.pem'),
  }),
};

function connect(options) {
  return new Promise((resolve, reject) => {
    const socket = tls.connect(options);
    socket.once('secureConnect', () => resolve(socket));
    socket.once('error', reject);
    socket.resume();
  });
}

async function connectAndCaptureSession(options) {
  const socket = tls.connect(options);
  const sessionPromise = once(socket, 'session').then(([session]) => session);

  socket.resume();
  await once(socket, 'secureConnect');

  assert.strictEqual(socket.authorized, true);
  assert.strictEqual(socket.isSessionReused(), false);

  const session = await sessionPromise;
  assert(session);

  await once(socket, 'close');
  return session;
}

async function testVersion(minVersion, maxVersion) {
  const server = tls.createServer({
    key: load('agent1-key.pem'),
    cert: load('agent1-cert.pem'),
    minVersion,
    maxVersion,
    SNICallback(servername, cb) {
      cb(null, contexts[servername] || contexts.agent1);
    },
  }, (socket) => {
    socket.end('ok');
  });

  server.listen(0);
  await once(server, 'listening');

  try {
    const port = server.address().port;
    const session = await connectAndCaptureSession({
      port,
      host: '127.0.0.1',
      servername: 'agent1',
      minVersion,
      maxVersion,
      ca: [load('ca1-cert.pem')],
    });

    await assert.rejects(
      connect({
        port,
        host: '127.0.0.1',
        servername: 'agent3',
        session,
        minVersion,
        maxVersion,
        ca: [load('ca1-cert.pem')],
      }).then((socket) => {
        socket.destroy();
        return socket;
      }),
      {
        code: 'UNABLE_TO_VERIFY_LEAF_SIGNATURE',
      },
    );

    await assert.rejects(
      connect({
        port,
        host: '127.0.0.1',
        servername: 'agent3',
        minVersion,
        maxVersion,
        ca: [load('ca1-cert.pem')],
      }).then((socket) => {
        socket.destroy();
        return socket;
      }),
      {
        code: 'UNABLE_TO_VERIFY_LEAF_SIGNATURE',
      },
    );
  } finally {
    server.close();
    await once(server, 'close');
  }
}

(async function() {
  await testVersion('TLSv1.2', 'TLSv1.2');
  await testVersion('TLSv1.3', 'TLSv1.3');
})().then(common.mustCall());
