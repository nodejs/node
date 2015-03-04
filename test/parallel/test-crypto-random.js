var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

// bump, we register a lot of exit listeners
process.setMaxListeners(256);

[crypto.randomBytes,
  crypto.pseudoRandomBytes
].forEach(function(f) {
  [-1,
    undefined,
    null,
    false,
    true,
    {},
    []
  ].forEach(function(value) {
    assert.throws(function() { f(value); });
    assert.throws(function() { f(value, function() {}); });
  });

  [0, 1, 2, 4, 16, 256, 1024].forEach(function(len) {
    f(len, checkCall(function(ex, buf) {
      assert.equal(null, ex);
      assert.equal(len, buf.length);
      assert.ok(Buffer.isBuffer(buf));
    }));
  });
});

// assert that the callback is indeed called
function checkCall(cb, desc) {
  var called_ = false;

  process.on('exit', function() {
    assert.equal(true, called_, desc || ('callback not called: ' + cb));
  });

  return function() {
    return called_ = true, cb.apply(cb, Array.prototype.slice.call(arguments));
  };
}

// #5126, "FATAL ERROR: v8::Object::SetIndexedPropertiesToExternalArrayData()
// length exceeds max acceptable value"
assert.throws(function() {
  crypto.randomBytes(0x3fffffff + 1);
}, TypeError);
