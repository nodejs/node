// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const fs = require('internal/fs');

assert.doesNotThrow(() => fs.assertEncoding());
assert.doesNotThrow(() => fs.assertEncoding('utf8'));
assert.throws(() => fs.assertEncoding('foo'), /^Error: Unknown encoding: foo$/);
