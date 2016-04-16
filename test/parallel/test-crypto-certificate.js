'use strict';
// Flags: --no-warnings

const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
const crypto = require('crypto');
const fs = require('fs');

crypto.DEFAULT_ENCODING = 'buffer';

// Test Certificates
const spkacValid = fs.readFileSync(common.fixturesDir + '/spkac.valid');
const spkacFail = fs.readFileSync(common.fixturesDir + '/spkac.fail');
const spkacPem = fs.readFileSync(common.fixturesDir + '/spkac.pem');

assert.equal(crypto.certVerifySpkac(spkacValid), true);
assert.equal(crypto.certVerifySpkac(spkacFail), false);

assert.equal(
  stripLineEndings(crypto.certExportPublicKey(spkacValid).toString('utf8')),
  stripLineEndings(spkacPem.toString('utf8'))
);
assert.equal(crypto.certExportPublicKey(spkacFail), '');

assert.equal(
  crypto.certExportChallenge(spkacValid).toString('utf8'),
  'fb9ab814-6677-42a4-a60c-f905d1a6924d'
);
assert.equal(crypto.certExportChallenge(spkacFail), '');

function stripLineEndings(obj) {
  return obj.replace(/\n/g, '');
}

process.on('warning', common.mustCall((warning) => {
  assert.equal(warning.name, 'DeprecationWarning');
  assert(/crypto.Certificate is deprecated/.test(warning.message));
}));
crypto.Certificate();
