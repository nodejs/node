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
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { Certificate } = crypto;
const fixtures = require('../common/fixtures');

// Test Certificates
const spkacValid = fixtures.readSync('spkac.valid');
const spkacFail = fixtures.readSync('spkac.fail');
const spkacPem = fixtures.readSync('spkac.pem');

function checkMethods(certificate) {

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
}

{
  // Test instance methods
  checkMethods(new Certificate());
}

{
  // Test static methods
  checkMethods(Certificate);
}

function stripLineEndings(obj) {
  return obj.replace(/\n/g, '');
}

// Direct call Certificate() should return instance
assert(Certificate() instanceof Certificate);

[1, {}, [], Infinity, true, 'test', undefined, null].forEach((val) => {
  assert.throws(
    () => Certificate.verifySpkac(val),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "spkac" argument must be one of type Buffer, TypedArray, ' +
               `or DataView. Received type ${typeof val}`
    }
  );
});

[1, {}, [], Infinity, true, undefined, null].forEach((val) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "spkac" argument must be one of type string, Buffer,' +
             ` TypedArray, or DataView. Received type ${typeof val}`
  };
  assert.throws(() => Certificate.exportPublicKey(val), errObj);
  assert.throws(() => Certificate.exportChallenge(val), errObj);
});
