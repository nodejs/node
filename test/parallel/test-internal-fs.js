// Flags: --expose-internals
'use strict';

const common = require('../common');
const fs = require('internal/fs/utils');

// Valid encodings and no args should not throw.
fs.assertEncoding();
fs.assertEncoding('utf8');

common.expectsError(
  () => fs.assertEncoding('foo'),
  { code: 'ERR_INVALID_OPT_VALUE_ENCODING', type: TypeError }
);
