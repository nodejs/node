'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (!process.features.tls_npn)
  common.skip('Skipping because node compiled without NPN feature of OpenSSL.');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}

const serverOptions = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
  crl: loadPEM('ca2-crl'),
  SNICallback: function(servername, cb) {
    cb(null, tls.createSecureContext({
      key: loadPEM('agent2-key'),
      cert: loadPEM('agent2-cert'),
      crl: loadPEM('ca2-crl'),
    }));
  },
  NPNProtocols: ['a', 'b', 'c']
};

const clientsOptions = [{
  port: undefined,
  key: serverOptions.key,
  cert: serverOptions.cert,
  crl: serverOptions.crl,
  NPNProtocols: ['a', 'b', 'c'],
  rejectUnauthorized: false
}, {
  port: undefined,
  key: serverOptions.key,
  cert: serverOptions.cert,
  crl: serverOptions.crl,
  NPNProtocols: ['c', 'b', 'e'],
  rejectUnauthorized: false
}, {
  port: undefined,
  key: serverOptions.key,
  cert: serverOptions.cert,
  crl: serverOptions.crl,
  rejectUnauthorized: false
}, {
  port: undefined,
  key: serverOptions.key,
  cert: serverOptions.cert,
  crl: serverOptions.crl,
  NPNProtocols: ['first-priority-unsupported', 'x', 'y'],
  rejectUnauthorized: false
}];

const serverResults = [];
const clientsResults = [];

const server = tls.createServer(serverOptions, function(c) {
  serverResults.push(c.npnProtocol);
});
server.listen(0, startTest);

function startTest() {
  function connectClient(options, callback) {
    options.port = server.address().port;
    const client = tls.connect(options, function() {
      clientsResults.push(client.npnProtocol);
      client.destroy();

      callback();
    });
  }

  connectClient(clientsOptions[0], function() {
    connectClient(clientsOptions[1], function() {
      connectClient(clientsOptions[2], function() {
        connectClient(clientsOptions[3], function() {
          server.close();
        });
      });
    });
  });
}

process.on('exit', function() {
  assert.strictEqual(serverResults[0], clientsResults[0]);
  assert.strictEqual(serverResults[1], clientsResults[1]);
  assert.strictEqual(serverResults[2], 'http/1.1');
  assert.strictEqual(clientsResults[2], false);
  assert.strictEqual(serverResults[3], 'first-priority-unsupported');
  assert.strictEqual(clientsResults[3], false);
});
