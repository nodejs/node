'use strict';
if (!process.features.tls_npn) {
  common.skip('node compiled without OpenSSL or ' +
              'with old OpenSSL version.');
  return;
}

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');


function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

var serverOptions = {
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

var clientsOptions = [{
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

var server = tls.createServer(serverOptions, function(c) {
  serverResults.push(c.npnProtocol);
});
server.listen(0, startTest);

function startTest() {
  function connectClient(options, callback) {
    options.port = server.address().port;
    var client = tls.connect(options, function() {
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
  assert.equal(serverResults[0], clientsResults[0]);
  assert.equal(serverResults[1], clientsResults[1]);
  assert.equal(serverResults[2], 'http/1.1');
  assert.equal(clientsResults[2], false);
  assert.equal(serverResults[3], 'first-priority-unsupported');
  assert.equal(clientsResults[3], false);
});
