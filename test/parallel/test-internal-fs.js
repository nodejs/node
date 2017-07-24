// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('internal/fs');

assert.doesNotThrow(() => fs.assertEncoding());
assert.doesNotThrow(() => fs.assertEncoding('utf8'));
common.expectsError(
  () => fs.assertEncoding('foo'),
  { code: 'ERR_INVALID_OPT_VALUE_ENCODING', type: TypeError }
);
