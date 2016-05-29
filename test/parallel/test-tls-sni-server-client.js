'use strict';
if (!process.features.tls_sni) {
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
  cert: loadPEM('agent2-cert')
};

var SNIContexts = {
  'a.example.com': {
    key: loadPEM('agent1-key'),
    cert: loadPEM('agent1-cert')
  },
  'asterisk.test.com': {
    key: loadPEM('agent3-key'),
    cert: loadPEM('agent3-cert')
  },
  'chain.example.com': {
    key: loadPEM('agent6-key'),
    // NOTE: Contains ca3 chain cert
    cert: loadPEM('agent6-cert')
  }
};

var clientsOptions = [{
  port: undefined,
  ca: [loadPEM('ca1-cert')],
  servername: 'a.example.com',
  rejectUnauthorized: false
}, {
  port: undefined,
  ca: [loadPEM('ca2-cert')],
  servername: 'b.test.com',
  rejectUnauthorized: false
}, {
  port: undefined,
  ca: [loadPEM('ca2-cert')],
  servername: 'a.b.test.com',
  rejectUnauthorized: false
}, {
  port: undefined,
  ca: [loadPEM('ca1-cert')],
  servername: 'c.wrong.com',
  rejectUnauthorized: false
}, {
  port: undefined,
  ca: [loadPEM('ca1-cert')],
  servername: 'chain.example.com',
  rejectUnauthorized: false
}];

const serverResults = [];
const clientResults = [];

var server = tls.createServer(serverOptions, function(c) {
  serverResults.push(c.servername);
});

server.addContext('a.example.com', SNIContexts['a.example.com']);
server.addContext('*.test.com', SNIContexts['asterisk.test.com']);
server.addContext('chain.example.com', SNIContexts['chain.example.com']);

server.listen(0, startTest);

function startTest() {
  var i = 0;
  function start() {
    // No options left
    if (i === clientsOptions.length)
      return server.close();

    var options = clientsOptions[i++];
    options.port = server.address().port;
    var client = tls.connect(options, function() {
      clientResults.push(
        client.authorizationError &&
        /Hostname\/IP doesn't/.test(client.authorizationError));
      client.destroy();

      // Continue
      start();
    });
  }

  start();
}

process.on('exit', function() {
  assert.deepStrictEqual(serverResults, [
    'a.example.com', 'b.test.com', 'a.b.test.com', 'c.wrong.com',
    'chain.example.com'
  ]);
  assert.deepStrictEqual(clientResults, [true, true, false, false, true]);
});
