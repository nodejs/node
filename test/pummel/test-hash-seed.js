'use strict';

// Check that spawn child don't create duplicated entries
require('common');
const REPETITIONS = 2;
const assert = require('assert');
const fixtures = require('common/fixtures');
const { spawnSync } = require('child_process');
const targetScript = fixtures.path('guess-hash-seed.js');
const seeds = [];

for (let i = 0; i < REPETITIONS; ++i) {
  const seed = spawnSync(process.execPath, [targetScript], {
    encoding: 'utf8'
  }).stdout.trim();
  seeds.push(seed);
}

console.log(`Seeds: ${seeds}`);
const hasDuplicates = new Set(seeds).size !== seeds.length;
assert.strictEqual(hasDuplicates, false);
