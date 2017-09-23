'use strict';

const common = require('../common');
const { readKey } = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const { OPENSSL_VERSION_NUMBER, SSL_OP_NO_TICKET } =
    require('crypto').constants;

const options = {
  key: readKey('agent1-key.pem'),
  cert: readKey('agent1-cert.pem'),
  secureOptions: SSL_OP_NO_TICKET
};

// Create TLS1.2 server
https.createServer(options, function(req, res) {
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

// Create TLS1 server
function faultyServer(port) {
  options.secureProtocol = 'TLSv1_method';
  https.createServer(options, function(req, res) {
    res.end('hello faulty');
  }).listen(port, function() {
    second(this);
  });
}

// Attempt to request using cached session
function second(server, session) {
  const req = https.request({
    port: server.address().port,
    rejectUnauthorized: false
  }, function(res) {
    res.resume();
  });

  if (OPENSSL_VERSION_NUMBER >= 0x10100000) {
    // Although we have a TLS 1.2 session to offer to the TLS 1.0 server,
    // connection to the TLS 1.0 server should work.
    req.on('response', common.mustCall(function(res) {
      // The test is now complete for OpenSSL 1.1.0.
      server.close();
    }));
  } else {
    // OpenSSL 1.0.x mistakenly locked versions based on the session it was
    // offering. This causes this sequent request to fail. Let it fail, but
    // test that this is mitigated on the next try by invalidating the session.
    req.on('error', common.mustCall(function(err) {
      assert(/wrong version number/.test(err.message));

      req.on('close', function() {
        third(server);
      });
    }));
  }
  req.end();
}

// Try one more time - session should be evicted!
function third(server) {
  const req = https.request({
    port: server.address().port,
    rejectUnauthorized: false
  }, function(res) {
    res.resume();
    assert(!req.socket.isSessionReused());
    server.close();
  });
  req.on('error', common.mustNotCall());
  req.end();
}
