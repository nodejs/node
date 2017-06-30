'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

// Test interaction of compiled-in CAs with user-provided CAs.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tls = require('tls');

function filenamePEM(n) {
  return path.join(common.fixturesDir, 'keys', `${n}.pem`);
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

const caCert = loadPEM('ca1-cert');

const opts = {
  host: 'www.nodejs.org',
  port: 443,
  rejectUnauthorized: true
};

// Success relies on the compiled in well-known root CAs
tls.connect(opts, common.mustCall(end));

// The .ca option replaces the well-known roots, so connection fails.
opts.ca = caCert;
tls.connect(opts, fail).on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'unable to get local issuer certificate');
}));

function fail() {
  assert.fail('should fail to connect');
}

// New secure contexts have the well-known root CAs.
opts.secureContext = tls.createSecureContext();
tls.connect(opts, common.mustCall(end));

// Explicit calls to addCACert() add to the default well-known roots, instead
// of replacing, so connection still succeeds.
opts.secureContext.context.addCACert(caCert);
tls.connect(opts, common.mustCall(end));

function end() {
  this.end();
}
