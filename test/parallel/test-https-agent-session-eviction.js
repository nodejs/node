// Flags: --tls-min-v1.0
'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const fixtures = require('../common/fixtures');
const { hasOpenSSL, hasFIPS } = require('../common/crypto');

const https = require('https');
const { constants: { SSL_OP_NO_TICKET } } = require('crypto');
const fips3 = hasFIPS(3);

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  secureOptions: SSL_OP_NO_TICKET,
};

if (fips3) {
  options.minVersion = 'TLSv1.3';
  options.maxVersion = 'TLSv1.3';
}

if (!process.features.openssl_is_boringssl) {
  options.ciphers = fips3 ?
    'ECDHE-RSA-AES256-GCM-SHA384' : 'RSA@SECLEVEL=0';
}

// Create the initial server and cache a session from it.
https.createServer(options, function(req, res) {
  res.writeHead(200, { 'Connection': 'close' });
  res.end('ohai');
}).listen(0, function() {
  first(this);
});

// Do request and let agent cache the session
function first(server) {
  const port = server.address().port;
  const req = https.request({
    port: port,
    rejectUnauthorized: false
  }, function(res) {
    res.resume();

    server.close(function() {
      faultyServer(port);
    });
  });
  req.end();
}

// Create a server constrained to a different TLS version.
function faultyServer(port) {
  if (fips3) {
    options.minVersion = 'TLSv1.2';
    options.maxVersion = 'TLSv1.2';
  } else {
    options.secureProtocol = 'TLSv1_method';
  }
  https.createServer(options, function(req, res) {
    res.writeHead(200, { 'Connection': 'close' });
    res.end('hello faulty');
  }).listen(port, function() {
    second(this);
  });
}

// Attempt to request using cached session
function second(server, session) {
  const req = https.request({
    port: server.address().port,
    ciphers: fips3 ? 'ECDHE-RSA-AES256-GCM-SHA384' :
      (hasOpenSSL(3, 1) ? 'DEFAULT:@SECLEVEL=0' : 'DEFAULT'),
    rejectUnauthorized: false
  }, function(res) {
    res.resume();
  });

  // Offering the cached session to a server using another TLS version should
  // not prevent a fresh connection.
  req.on('response', common.mustCall(function(res) {
    // The test is now complete for OpenSSL 1.1.0.
    server.close();
  }));

  req.end();
}
