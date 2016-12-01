'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

var fs = require('fs');

// Test Certificates
var spkacValid = fs.readFileSync(common.fixturesDir + '/spkac.valid');
var spkacFail = fs.readFileSync(common.fixturesDir + '/spkac.fail');
var spkacPem = fs.readFileSync(common.fixturesDir + '/spkac.pem');

var certificate = new crypto.Certificate();

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
