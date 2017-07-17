'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const fn = fixtures.path('elipses.txt');

const s = fs.readFileSync(fn, 'utf8');
for (let i = 0; i < s.length; i++) {
  assert.strictEqual('\u2026', s[i]);
}
assert.strictEqual(10000, s.length);
