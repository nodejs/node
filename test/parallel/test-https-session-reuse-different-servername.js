'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const https = require('https');
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

function request(options) {
  return new Promise((resolve, reject) => {
    const req = https.get({
      ...options,
      host: '127.0.0.1',
      agent: false,
    }, (res) => {
      res.resume();
      res.once('end', () => resolve(res));
    });

    req.once('error', reject);
  });
}

async function requestAndCaptureSession(options) {
  let sessionResolve;
  const sessionPromise = new Promise((resolve) => {
    sessionResolve = resolve;
  });

  const req = https.get({
    ...options,
    host: '127.0.0.1',
    agent: false,
  });

  req.on('socket', (socket) => {
    socket.once('session', sessionResolve);
  });

  const res = await new Promise((resolve, reject) => {
    req.once('response', resolve);
    req.once('error', reject);
  });

  assert.strictEqual(res.socket.authorized, true);
  assert.strictEqual(res.socket.isSessionReused(), false);

  res.resume();
  await once(res, 'end');

  const session = await sessionPromise;
  assert(session);
  return session;
}

const server = https.createServer({
  key: load('agent1-key.pem'),
  cert: load('agent1-cert.pem'),
  minVersion: 'TLSv1.2',
  maxVersion: 'TLSv1.2',
  SNICallback(servername, cb) {
    cb(null, contexts[servername] || contexts.agent1);
  },
}, (req, res) => {
  res.end('ok');
});

(async function() {
  server.listen(0);
  await once(server, 'listening');

  try {
    const session = await requestAndCaptureSession({
      port: server.address().port,
      servername: 'agent1',
      rejectUnauthorized: true,
      ca: [load('ca1-cert.pem')],
    });

    await assert.rejects(
      request({
        port: server.address().port,
        servername: 'agent3',
        session,
        rejectUnauthorized: true,
        ca: [load('ca1-cert.pem')],
      }),
      {
        code: 'UNABLE_TO_VERIFY_LEAF_SIGNATURE',
      },
    );

    await assert.rejects(
      request({
        port: server.address().port,
        servername: 'agent3',
        rejectUnauthorized: true,
        ca: [load('ca1-cert.pem')],
      }),
      {
        code: 'UNABLE_TO_VERIFY_LEAF_SIGNATURE',
      },
    );
  } finally {
    server.close();
    await once(server, 'close');
  }
})().then(common.mustCall());
