'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const { isBoringSSL } = require('../common/crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const ca = fixtures.readKey('name-constraints-ca-cert.pem');
const key = fixtures.readKey('agent1-key.pem');

// A DNS SAN prevents CN fallback; an email SAN does not. OpenSSL constrains
// the CN whenever hostname verification can use it. BoringSSL instead rejects
// DNS-like CNs when the SAN extension is absent, but does not constrain the CN
// when an email SAN is present, so that case remains authorized.
const constraintError = isBoringSSL ?
  /Issuer has name constraints but leaf has no SANs/ :
  /permitted subtree violation/;
for (const version of ['TLSv1.2', 'TLSv1.3']) {
  for (const [name, servername, valid] of [
    ['permitted', 'www.example.com', !isBoringSSL],
    ['excluded', 'outside.invalid', false],
    ['dns-san', 'www.example.com', true],
    ['email-san', 'outside.invalid', isBoringSSL],
  ]) {
    for (const rejectUnauthorized of [true, false]) {
      const rejected = !valid && rejectUnauthorized;
      const server = tls.createServer({
        key,
        cert: fixtures.readKey(`name-constraints-${name}-cert.pem`),
        minVersion: version,
        maxVersion: version,
      }, (socket) => socket.end());
      server.on('tlsClientError', () => {});
      server.listen(0, common.mustCall(() => {
        const client = tls.connect({
          port: server.address().port,
          ca,
          servername,
          rejectUnauthorized,
        });
        client.on('secureConnect', rejected ? common.mustNotCall() : common.mustCall(() => {
          assert.strictEqual(client.authorized, valid);
          if (!valid) assert.strictEqual(client.authorizationError, 'UNSPECIFIED');
          client.end();
        }));
        client.on('error', rejected ? common.mustCall((err) => {
          assert.strictEqual(err.code, 'UNSPECIFIED');
          assert.match(err.message, constraintError);
        }) : common.mustNotCall());
        client.on('close', common.mustCall(() => server.close()));
      }));
    }
  }
}
