// Flags: --no-deprecation
'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const path = require('path');

const dir = fixtures.path('GH-7131');
const b = require(path.join(dir, 'b'));
const a = require(path.join(dir, 'a'));

assert.strictEqual(a.length, 1);
assert.strictEqual(b.length, 0);
assert.deepStrictEqual(a[0].exports, b);
