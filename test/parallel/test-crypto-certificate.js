'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');

crypto.DEFAULT_ENCODING = 'buffer';

// Test Certificates
const spkacValid = fixtures.readSync('spkac.valid');
const spkacFail = fixtures.readSync('spkac.fail');
const spkacPem = fixtures.readSync('spkac.pem');

const certificate = new crypto.Certificate();

assert.strictEqual(certificate.verifySpkac(spkacValid), true);
assert.strictEqual(certificate.verifySpkac(spkacFail), false);

assert.strictEqual(
  stripLineEndings(certificate.exportPublicKey(spkacValid).toString('utf8')),
  stripLineEndings(spkacPem.toString('utf8'))
);
assert.strictEqual(certificate.exportPublicKey(spkacFail), '');

assert.strictEqual(
  certificate.exportChallenge(spkacValid).toString('utf8'),
  'fb9ab814-6677-42a4-a60c-f905d1a6924d'
);
assert.strictEqual(certificate.exportChallenge(spkacFail), '');

function stripLineEndings(obj) {
  return obj.replace(/\n/g, '');
}

// direct call Certificate() should return instance
assert(crypto.Certificate() instanceof crypto.Certificate);
