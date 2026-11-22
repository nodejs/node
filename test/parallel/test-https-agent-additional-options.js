'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const https = require('https');
const { hasFIPS, isBoringSSL } = require('../common/crypto');
const fixtures = require('../common/fixtures');
const fips3 = hasFIPS(3);

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  ca: fixtures.readKey('ca1-cert.pem'),
  minVersion: fips3 ? 'TLSv1.2' : 'TLSv1.1',
};

if (!isBoringSSL) {
  options.ciphers = fips3 ?
    'ECDHE-RSA-AES256-GCM-SHA384' : 'ALL@SECLEVEL=0';
}

const server = https.Server(options, (req, res) => {
  res.writeHead(200);
  res.end('hello world\n');
});

function getBaseOptions(port) {
  const baseOptions = {
    path: '/',
    port: port,
    ca: options.ca,
    rejectUnauthorized: true,
    servername: 'agent1',
  };

  if (!isBoringSSL) {
    baseOptions.ciphers = fips3 ?
      'ECDHE-RSA-AES256-GCM-SHA384' : 'ALL@SECLEVEL=0';
  }

  return baseOptions;
}

const updatedValues = new Map([
  ['dhparam', fixtures.readKey('dh2048.pem')],
  ['ecdhCurve', 'secp384r1'],
  ['honorCipherOrder', true],
  ['minVersion', fips3 ? 'TLSv1.2' : 'TLSv1.1'],
  ['maxVersion', 'TLSv1.3'],
  ['secureOptions', crypto.constants.SSL_OP_CIPHER_SERVER_PREFERENCE],
  ['secureProtocol', fips3 ?
    'TLSv1_2_method' : 'TLSv1_1_method'],
  ['sessionIdContext', 'sessionIdContext'],
]);

// Every variation must be pooled under its own name, and repeating a
// variation must reuse the name it had.
const requested = new Set();
let expectedNames = 1;
function variations(iter, port, cb) {
  return common.mustCall((res) => {
    res.resume();
    https.globalAgent.once('free', common.mustCall(() => {
      // Verify that the connections are in the freeSockets pool, one name for
      // each distinct set of options.
      const keys = Object.keys(https.globalAgent.freeSockets);
      assert.strictEqual(keys.length, expectedNames, `unexpected ${keys}`);
      const next = iter.next();

      if (next.done) {
        https.globalAgent.destroy();
        server.close();
      } else {
        const [key, val] = next.value;
        if (!requested.has(key)) {
          requested.add(key);
          expectedNames++;
        }
        https.get({ ...getBaseOptions(port), [key]: val },
                  variations(iter, port, cb));
      }
    }));
  });
}

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  https.globalAgent.keepAlive = true;
  const all = [...updatedValues.entries()];
  const entries = [...all, all[0]];
  https.get(getBaseOptions(port), variations(entries.values(), port));
}));
