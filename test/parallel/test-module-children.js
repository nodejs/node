// Flags: --no-deprecation
'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');

const dir = path.join(common.fixturesDir, 'GH-7131');
const b = require(path.join(dir, 'b'));
const a = require(path.join(dir, 'a'));

assert.strictEqual(a.length, 1);
assert.strictEqual(b.length, 0);
assert.deepStrictEqual(a[0].exports, b);
