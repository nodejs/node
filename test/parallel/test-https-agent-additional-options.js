// Flags: --tls-v1.1
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
  ca: fixtures.readKey('ca1-cert.pem')
};

const server = https.Server(options, function(req, res) {
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
  };
}

const updatedValues = new Map([
  ['dhparam', fixtures.readKey('dh2048.pem')],
  ['ecdhCurve', 'secp384r1'],
  ['honorCipherOrder', true],
  ['secureOptions', crypto.constants.SSL_OP_CIPHER_SERVER_PREFERENCE],
  ['secureProtocol', 'TLSv1_1_method'],
  ['sessionIdContext', 'sessionIdContext'],
]);

function variations(iter, port, cb) {
  const { done, value } = iter.next();
  if (done) {
    return common.mustCall(cb);
  } else {
    const [key, val] = value;
    return common.mustCall(function(res) {
      res.resume();
      https.globalAgent.once('free', common.mustCall(function() {
        https.get(
          Object.assign({}, getBaseOptions(port), { [key]: val }),
          variations(iter, port, cb)
        );
      }));
    });
  }
}

server.listen(0, common.mustCall(function() {
  const port = this.address().port;
  const globalAgent = https.globalAgent;
  globalAgent.keepAlive = true;
  https.get(getBaseOptions(port), variations(
    updatedValues.entries(),
    port,
    common.mustCall(function(res) {
      res.resume();
      globalAgent.once('free', common.mustCall(function() {
        // Verify that different keep-alived connections are created
        // for the base call and each variation
        const keys = Object.keys(globalAgent.freeSockets);
        assert.strictEqual(keys.length, 1 + updatedValues.size);
        let i = 1;
        for (const [, value] of updatedValues) {
          assert.ok(
            keys[i].startsWith(value.toString() + ':') ||
            keys[i].endsWith(':' + value.toString()) ||
            keys[i].includes(':' + value.toString() + ':')
          );
          i++;
        }
        globalAgent.destroy();
        server.close();
      }));
    })
  ));
}));
