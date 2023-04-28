'use strict';
// This tests that the internal flags in URL objects are consistent, as manifest
// through assert libraries.
// See https://github.com/nodejs/node/issues/24211

// Tests below are not from WPT.

require('../common');
const assert = require('assert');

assert.deepStrictEqual(
  new URL('./foo', 'https://example.com/'),
  new URL('https://example.com/foo')
);
assert.deepStrictEqual(
  new URL('./foo', 'https://user:pass@example.com/'),
  new URL('https://user:pass@example.com/foo')
);
