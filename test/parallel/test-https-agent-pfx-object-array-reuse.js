'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const { hasFIPS } = require('../common/crypto');
const fixtures = require('../common/fixtures');
const fips3 = hasFIPS(3);
const fips35 = hasFIPS(3, 5);

const onRequest = (req, res) => {
  res.end(req.socket.getPeerCertificate().subject.CN);
};
const requestHandler = fips3 && !fips35 ?
  common.mustNotCall() :
  common.mustCall(onRequest, fips3 ? 1 : 2);

const server = https.createServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
  requestCert: true,
  rejectUnauthorized: false,
}, requestHandler);

server.listen(0, common.mustCall(async () => {
  const agent = new https.Agent({ keepAlive: true, maxSockets: 1 });
  const port = server.address().port;

  if (fips3) {
    await assert.rejects(request({
      agent,
      port,
      pfx: [{ buf: fixtures.readKey('agent1.pfx'), passphrase: 'sample' }],
    }, false), { code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION' });

    if (!fips35) {
      agent.destroy();
      server.close();
      return;
    }

    const result = await request({
      agent,
      port,
      pfx: [{
        buf: fixtures.readKey('agent1-fips.pfx'),
        passphrase: 'password',
      }],
    });
    assert.strictEqual(result.body, 'agent1');
    assert.strictEqual(result.reusedSocket, false);
    agent.destroy();
    server.close();
    return;
  }

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

function request(options, expectResponse = true) {
  return new Promise((resolve, reject) => {
    const onResponse = expectResponse ? common.mustCall((res) => {
      let body = '';
      res.setEncoding('utf8');
      res.on('data', (chunk) => body += chunk);
      res.on('end', common.mustCall(() => {
        resolve({ body, reusedSocket: req.reusedSocket });
      }));
    }) : common.mustNotCall();
    const req = https.get({
      ...options,
      rejectUnauthorized: false,
    }, onResponse);
    req.on('error', reject);
  });
}
