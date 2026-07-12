'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { X509Certificate } = require('crypto');
const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const defaultCredentials = {
  cert: fixtures.readKey('ca5-cert.pem'),
  key: fixtures.readKey('ca5-key.pem'),
};
const sniCredentials = {
  cert: fixtures.readKey('agent1-cert.pem'),
  key: fixtures.readKey('agent1-key.pem'),
};

const defaultCertificate = new X509Certificate(defaultCredentials.cert);
const sniCertificate = new X509Certificate(sniCredentials.cert);
const sniContext = tls.createSecureContext(sniCredentials);

function request(port, servername, expectedCertificate) {
  return new Promise((resolve, reject) => {
    const req = https.get({
      host: '127.0.0.1',
      port,
      servername,
      rejectUnauthorized: false,
      agent: false,
    }, common.mustCall((response) => {
      try {
        const certificate = response.socket.getPeerX509Certificate();
        assert.strictEqual(certificate.fingerprint256,
                           expectedCertificate.fingerprint256);
      } catch (err) {
        reject(err);
        return;
      }

      response.resume();
      response.once('end', resolve);
      response.once('error', reject);
    }));
    req.once('error', reject);
  });
}

const server = https.createServer({
  cert: defaultCredentials.cert,
  key: defaultCredentials.key,
  SNICallback: common.mustCall((servername, callback) => {
    assert.strictEqual(servername, 'agent1.com');
    callback(null, sniContext);
  }, 1),
}, (_request, response) => {
  response.end('ok');
});

server.listen(0, common.mustCall(async () => {
  try {
    const { port } = server.address();
    await request(port, undefined, defaultCertificate);
    await request(port, 'agent1.com', sniCertificate);
  } finally {
    server.close(common.mustCall());
  }
}));
