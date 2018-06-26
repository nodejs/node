'use strict';

require('../common');
const binding = process.binding('constants');
const constants = require('constants');
const assert = require('assert');

assert.ok(binding);
assert.ok(binding.os);
assert.ok(binding.os.signals);
assert.ok(binding.os.errno);
assert.ok(binding.fs);
assert.ok(binding.crypto);

['os', 'fs', 'crypto'].forEach((l) => {
  Object.keys(binding[l]).forEach((k) => {
    if (typeof binding[l][k] === 'object') { // errno and signals
      Object.keys(binding[l][k]).forEach((j) => {
        assert.strictEqual(binding[l][k][j], constants[j]);
      });
    }
    if (l !== 'os') { // top level os constant isn't currently copied
      assert.strictEqual(binding[l][k], constants[k]);
    }
  });
});

assert.ok(Object.isFrozen(constants));
