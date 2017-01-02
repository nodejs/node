'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const fn = path.join(common.fixturesDir, 'elipses.txt');

const s = fs.readFileSync(fn, 'utf8');
for (let i = 0; i < s.length; i++) {
  assert.strictEqual('\u2026', s[i]);
}
assert.strictEqual(10000, s.length);
