'use strict';
// This tests that the context of URL objects are not
// enumerable and thus considered by assert libraries.
// See https://github.com/nodejs/node/issues/24211

// Tests below are not from WPT.

require('../common');
const assert = require('assert');

assert.deepStrictEqual(
  new URL('./foo', 'https://example.com/'),
  new URL('https://example.com/foo')
);
