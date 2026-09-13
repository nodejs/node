'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const { isBoringSSL } = require('../common/crypto');
// BoringSSL rejects DNS-like CNs without SANs under name constraints,
// even when the CN is permitted by those constraints.
if (isBoringSSL) common.skip('requires OpenSSL CN name constraints');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const ca = fixtures.readKey('name-constraints-ca-cert.pem');
const key = fixtures.readKey('agent1-key.pem');

// A DNS SAN prevents CN fallback; an email SAN does not. Name constraints
// must cover the CN whenever hostname verification can use it.
for (const version of ['TLSv1.2', 'TLSv1.3']) {
  for (const [name, servername, valid] of [
    ['permitted', 'www.example.com', true],
    ['excluded', 'outside.invalid', false],
    ['dns-san', 'www.example.com', true],
    ['email-san', 'outside.invalid', false],
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
          assert.match(err.message, /permitted subtree violation/);
        }) : common.mustNotCall());
        client.on('close', common.mustCall(() => server.close()));
      }));
    }
  }
}
