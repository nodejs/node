'use strict';

require('../common');
const assert = require('assert');
const path = require('path');

const samples = [
  ['file*.txt', 'file[*].txt'],
  ['file?.txt', 'file[?].txt'],
  ['file[abc].txt', 'file[[]abc[]].txt'],
  ['folder/**/*.txt', 'folder/[*][*]/[*].txt'],
  ['file{1,2}.txt', 'file[{]1,2[}].txt'],
  ['file[0-9]?.txt', 'file[[]0-9[]][?].txt'],
  ['C:\\Users\\*.txt', 'C:\\Users\\[*].txt'],
  ['?[]', '[?][[][]]'],
];

for (const [expected, escapedPattern] of samples) {
  const actual = path.unescapeGlob(escapedPattern);
  assert.strictEqual(actual, expected, `Expected ${escapedPattern} to be unescaped as ${expected}, got ${actual} instead`);
}

// Test for non-string input
assert.throws(() => path.unescapeGlob(123), /.*must be of type string.*/);
