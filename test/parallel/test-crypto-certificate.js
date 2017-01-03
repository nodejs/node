// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

const fs = require('fs');

// Test Certificates
const spkacValid = fs.readFileSync(common.fixturesDir + '/spkac.valid');
const spkacFail = fs.readFileSync(common.fixturesDir + '/spkac.fail');
const spkacPem = fs.readFileSync(common.fixturesDir + '/spkac.pem');

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
