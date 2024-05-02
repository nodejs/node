'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const https = require('https');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  ca: fixtures.readKey('ca1-cert.pem'),
  minVersion: 'TLSv1.1',
  ciphers: 'ALL@SECLEVEL=0'
};

const server = https.Server(options, (req, res) => {
  res.writeHead(200);
  res.end('hello world\n');
});

function getBaseOptions(port) {
  return {
    path: '/',
    port: port,
    ca: options.ca,
    rejectUnauthorized: true,
    servername: 'agent1',
    ciphers: 'ALL@SECLEVEL=0'
  };
}

const updatedValues = new Map([
  ['dhparam', fixtures.readKey('dh2048.pem')],
  ['ecdhCurve', 'secp384r1'],
  ['honorCipherOrder', true],
  ['minVersion', 'TLSv1.1'],
  ['maxVersion', 'TLSv1.3'],
  ['secureOptions', crypto.constants.SSL_OP_CIPHER_SERVER_PREFERENCE],
  ['secureProtocol', 'TLSv1_1_method'],
  ['sessionIdContext', 'sessionIdContext'],
]);

let value;
function variations(iter, port, cb) {
  return common.mustCall((res) => {
    res.resume();
    https.globalAgent.once('free', common.mustCall(() => {
      // Verify that the most recent connection is in the freeSockets pool.
      const keys = Object.keys(https.globalAgent.freeSockets);
      if (value) {
        assert.ok(
          keys.some((val) => val.startsWith(value.toString() + ':') ||
                            val.endsWith(':' + value.toString()) ||
                            val.includes(':' + value.toString() + ':')),
          `missing value: ${value.toString()} in ${keys}`
        );
      }
      const next = iter.next();

      if (next.done) {
        https.globalAgent.destroy();
        server.close();
      } else {
        // Save `value` for check the next time.
        const [key, val] = next.value;
        value = val;
        https.get({ ...getBaseOptions(port), [key]: val },
                  variations(iter, port, cb));
      }
    }));
  });
}

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  https.globalAgent.keepAlive = true;
  https.get(getBaseOptions(port), variations(updatedValues.entries(), port));
}));
