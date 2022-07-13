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
const spkacValid = fixtures.readKey('rsa_spkac.spkac');
const spkacChallenge = 'this-is-a-challenge';
const spkacFail = fixtures.readKey('rsa_spkac_invalid.spkac');
const spkacPublicPem = fixtures.readKey('rsa_public.pem');

function copyArrayBuffer(buf) {
  return buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
}

function checkMethods(certificate) {

  assert.strictEqual(certificate.verifySpkac(spkacValid), true);
  assert.strictEqual(certificate.verifySpkac(spkacFail), false);

  assert.strictEqual(
    stripLineEndings(certificate.exportPublicKey(spkacValid).toString('utf8')),
    stripLineEndings(spkacPublicPem.toString('utf8'))
  );
  assert.strictEqual(certificate.exportPublicKey(spkacFail), '');

  assert.strictEqual(
    certificate.exportChallenge(spkacValid).toString('utf8'),
    spkacChallenge
  );
  assert.strictEqual(certificate.exportChallenge(spkacFail), '');

  const ab = copyArrayBuffer(spkacValid);
  assert.strictEqual(certificate.verifySpkac(ab), true);
  assert.strictEqual(certificate.verifySpkac(new Uint8Array(ab)), true);
  assert.strictEqual(certificate.verifySpkac(new DataView(ab)), true);
}

{
  // Test maximum size of input buffer
  let buf;
  let skip = false;
  try {
    buf = Buffer.alloc(2 ** 31);
  } catch {
    // The allocation may fail on some systems. That is expected due
    // to architecture and memory constraints. If it does, go ahead
    // and skip this test.
    skip = true;
  }
  if (!skip) {
    assert.throws(
      () => Certificate.verifySpkac(buf), {
        code: 'ERR_OUT_OF_RANGE'
      });
    assert.throws(
      () => Certificate.exportChallenge(buf), {
        code: 'ERR_OUT_OF_RANGE'
      });
    assert.throws(
      () => Certificate.exportPublicKey(buf), {
        code: 'ERR_OUT_OF_RANGE'
      });
  }
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

[1, {}, [], Infinity, true, undefined, null].forEach((val) => {
  assert.throws(
    () => Certificate.verifySpkac(val),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
});

[1, {}, [], Infinity, true, undefined, null].forEach((val) => {
  const errObj = { code: 'ERR_INVALID_ARG_TYPE' };
  assert.throws(() => Certificate.exportPublicKey(val), errObj);
  assert.throws(() => Certificate.exportChallenge(val), errObj);
});
