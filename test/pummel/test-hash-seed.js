'use strict';

const REPETITIONS = 2;

const assert = require('assert');
const common = require('../common');
const cp = require('child_process');
const path = require('path');
const targetScript = path.resolve(common.fixturesDir, 'guess-hash-seed.js');
const seeds = [];

for (let i = 0; i < REPETITIONS; ++i) {
  const seed = cp.spawnSync(process.execPath, [targetScript],
                            { encoding: 'utf8' }).stdout.trim();
  seeds.push(seed);
}

console.log(`Seeds: ${seeds}`);
const hasDuplicates = (new Set(seeds)).size !== seeds.length;
assert.strictEqual(hasDuplicates, false);
