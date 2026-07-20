'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const fixtures = require('../common/fixtures');

const server = https.createServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
  requestCert: true,
  rejectUnauthorized: false,
}, common.mustCall((req, res) => {
  res.end(req.socket.getPeerCertificate().subject.CN);
}, 2));

server.listen(0, common.mustCall(async () => {
  const agent = new https.Agent({ keepAlive: true, maxSockets: 1 });
  const port = server.address().port;

  const first = await request({
    agent,
    port,
    pfx: [{ buf: fixtures.readKey('agent1.pfx'), passphrase: 'sample' }],
  });
  assert.strictEqual(first.body, 'agent1');
  assert.strictEqual(first.reusedSocket, false);

  const second = await request({
    agent,
    port,
    pfx: [{ buf: fixtures.readKey('agent10.pfx'), passphrase: 'sample' }],
  });
  assert.strictEqual(second.body, 'agent10.example.com');
  assert.strictEqual(second.reusedSocket, false);

  agent.destroy();
  server.close();
}));

function request(options) {
  return new Promise((resolve, reject) => {
    const req = https.get({
      ...options,
      rejectUnauthorized: false,
    }, common.mustCall((res) => {
      let body = '';
      res.setEncoding('utf8');
      res.on('data', (chunk) => body += chunk);
      res.on('end', common.mustCall(() => {
        resolve({ body, reusedSocket: req.reusedSocket });
      }));
    }));
    req.on('error', reject);
  });
}
