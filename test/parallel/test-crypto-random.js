'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const assert = require('assert');
const crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

// bump, we register a lot of exit listeners
process.setMaxListeners(256);

const expectedErrorRegexp = /^TypeError: size must be a number >= 0$/;
[crypto.randomBytes, crypto.pseudoRandomBytes].forEach(function(f) {
  [-1, undefined, null, false, true, {}, []].forEach(function(value) {
    assert.throws(function() { f(value); }, expectedErrorRegexp);
    assert.throws(function() { f(value, function() {}); }, expectedErrorRegexp);
  });

  [0, 1, 2, 4, 16, 256, 1024].forEach(function(len) {
    f(len, common.mustCall(function(ex, buf) {
      assert.strictEqual(ex, null);
      assert.strictEqual(buf.length, len);
      assert.ok(Buffer.isBuffer(buf));
    }));
  });
});

// #5126, "FATAL ERROR: v8::Object::SetIndexedPropertiesToExternalArrayData()
// length exceeds max acceptable value"
assert.throws(function() {
  crypto.randomBytes((-1 >>> 0) + 1);
}, TypeError);
