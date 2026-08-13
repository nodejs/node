'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const h2 = require('http2');
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

function makeRequest(server, authority, options) {
  return new Promise((resolve, reject) => {
    let session;
    let settled = false;

    function done(err, result) {
      if (settled)
        return;
      settled = true;

      if (err) {
        client.destroy();
        reject(err);
        return;
      }

      client.close();
      resolve({ session, result });
    }

    const client = h2.connect(authority, {
      ...options,
      ALPNProtocols: ['h2'],
      createConnection(url, connectOptions) {
        const socket = tls.connect({
          ...connectOptions,
          ALPNProtocols: ['h2'],
          host: '127.0.0.1',
          port: server.address().port,
          servername: connectOptions.servername || url.hostname,
        });

        socket.once('session', (value) => {
          if (session === undefined)
            session = value;
        });

        return socket;
      },
    });

    client.once('error', (err) => done(err));
    client.once('connect', () => {
      const req = client.request({ ':path': '/' });
      req.setEncoding('utf8');
      req.on('response', () => {});
      req.on('error', (err) => done(err));
      req.on('data', () => {});
      req.on('end', () => {
        done(null, {
          authority: client.socket.servername,
          authorized: client.socket.authorized,
          reused: client.socket.isSessionReused(),
          authorizationError: client.socket.authorizationError,
        });
      });
      req.end();
    });
  });
}

const server = h2.createSecureServer({
  key: load('agent1-key.pem'),
  cert: load('agent1-cert.pem'),
  allowHTTP1: false,
  minVersion: 'TLSv1.2',
  maxVersion: 'TLSv1.2',
  SNICallback(servername, cb) {
    cb(null, contexts[servername] || contexts.agent1);
  },
});

server.on('stream', (stream) => {
  stream.respond({ ':status': 200 });
  stream.end('ok');
});

(async function() {
  server.listen(0);
  await once(server, 'listening');

  try {
    const port = server.address().port;
    const first = await makeRequest(server, `https://agent1:${port}`, {
      ca: [load('ca1-cert.pem')],
      minVersion: 'TLSv1.2',
      maxVersion: 'TLSv1.2',
    });

    assert.strictEqual(first.result.authorized, true);
    assert.strictEqual(first.result.reused, false);
    assert(first.session);

    await assert.rejects(
      makeRequest(server, `https://agent3:${port}`, {
        ca: [load('ca1-cert.pem')],
        minVersion: 'TLSv1.2',
        maxVersion: 'TLSv1.2',
        session: first.session,
      }),
      {
        code: 'UNABLE_TO_VERIFY_LEAF_SIGNATURE',
      },
    );

    await assert.rejects(
      makeRequest(server, `https://agent3:${port}`, {
        ca: [load('ca1-cert.pem')],
        minVersion: 'TLSv1.2',
        maxVersion: 'TLSv1.2',
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
