// Flags: --expose-internals
'use strict';

require('../common');
const { internalBinding } = require('internal/test/binding');
const binding = internalBinding('constants');
const constants = require('constants');
const assert = require('assert');

assert.ok(binding);
assert.ok(binding.os);
assert.ok(binding.os.signals);
assert.ok(binding.os.errno);
assert.ok(binding.fs);
assert.ok(binding.crypto);

['os', 'fs', 'crypto'].forEach((l) => {
  for (const k of Object.keys(binding[l])) {
    if (typeof binding[l][k] === 'object') { // errno and signals
      for (const j of Object.keys(binding[l][k])) {
        assert.strictEqual(binding[l][k][j], constants[j]);
      }
    }
    if (l !== 'os') { // Top level os constant isn't currently copied
      assert.strictEqual(binding[l][k], constants[k]);
    }
  }
});

assert.ok(Object.isFrozen(constants));
