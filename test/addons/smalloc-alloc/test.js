var assert = require('assert');
var binding = require('./build/Release/binding');
var obj = binding.alloc(16);
for (var i = 0; i < 16; i++) {
  assert.ok(typeof obj[i] == 'number');
}
assert.ok(binding.hasExternalData(obj));
binding.dispose(obj);
assert.ok(typeof obj[0] !== 'number');
