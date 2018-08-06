// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const uv = internalBinding('uv');

// Ensures that the `UV_...` values in internalBinding('uv')
// are constants.

const keys = Object.keys(uv);
keys.forEach((key) => {
  if (key.startsWith('UV_')) {
    const val = uv[key];
    assert.throws(() => uv[key] = 1,
                  /^TypeError: Cannot assign to read only property/);
    assert.strictEqual(uv[key], val);
  }
});
