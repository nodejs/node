'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const https = require('https');
const { once } = require('events');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');
const ca1 = fixtures.readKey('ca1-cert.pem');
const ca2 = fixtures.readKey('ca2-cert.pem');

const server = https.createServer(
  { key, cert, minVersion: 'TLSv1.2', maxVersion: 'TLSv1.2' },
  (req, res) => res.end('ok'),
);

function request(port, options) {
  return new Promise((resolve, reject) => {
    const req = https.get({ host: '127.0.0.1', port, ...options },
      (res) => { res.resume(); res.on('end', resolve); });
    req.on('error', reject);
  });
}

(async function main() {
  server.listen(0);
  await once(server, 'listening');
  const port = server.address().port;

  // A per-request `rejectUnauthorized: true` must override an agent that
  // disables verification.
  {
    const agent = new https.Agent({ keepAlive: true, rejectUnauthorized: false });
    await request(port, { agent });
    await assert.rejects(
      request(port, { agent, rejectUnauthorized: true, servername: 'agent1' }),
      { code: 'UNABLE_TO_VERIFY_LEAF_SIGNATURE' },
    );
    agent.destroy();
  }

  // A per-request narrowed `ca` must override an agent that trusts a broader
  // set of CAs.
  {
    const agent = new https.Agent({ keepAlive: true, ca: [ca1] });
    await request(port, { agent, servername: 'agent1' });
    await assert.rejects(
      request(port, { agent, servername: 'agent1', ca: [ca2] }),
      { code: 'UNABLE_TO_VERIFY_LEAF_SIGNATURE' },
    );
    agent.destroy();
  }

  // A per-request `servername` must override an agent that pins a different
  // servername.
  {
    const agent = new https.Agent({ keepAlive: true, ca: [ca1], servername: 'agent1' });
    await request(port, { agent });
    await assert.rejects(
      request(port, { agent, servername: 'wronghost' }),
      { code: 'ERR_TLS_CERT_ALTNAME_INVALID' },
    );
    agent.destroy();
  }

  server.close();
  await once(server, 'close');
})().then(common.mustCall());
