// Flags: --experimental-modules
'use strict';
require('../common');
const assert = require('assert');
exports.asdf = 'asdf';
assert.strictEqual(require.main.exports.asdf, 'asdf');
