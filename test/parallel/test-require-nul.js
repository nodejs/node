'use strict';

require('../common');
const assert = require('assert');

// Nul bytes should throw, not abort.
assert.throws(() => require('\u0000ab'), /Cannot find module '\u0000ab'/);
assert.throws(() => require('a\u0000b'), /Cannot find module 'a\u0000b'/);
assert.throws(() => require('ab\u0000'), /Cannot find module 'ab\u0000'/);
