'use strict';

// Check that spawn child doesn't create duplicated entries
require('../common');
const Countdown = require('../common/countdown');
const REPETITIONS = 2;
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const targetScript = fixtures.path('guess-hash-seed.js');
const seeds = [];

const requiredCallback = () => {
  console.log(`Seeds: ${seeds}`);
  assert.strictEqual(new Set(seeds).size, seeds.length);
  assert.strictEqual(seeds.length, REPETITIONS);
};

const countdown = new Countdown(REPETITIONS, requiredCallback);

for (let i = 0; i < REPETITIONS; ++i) {
  let result = '';
  const subprocess = spawn(process.execPath, [targetScript]);
  subprocess.stdout.setEncoding('utf8');
  subprocess.stdout.on('data', (data) => { result += data; });

  subprocess.on('exit', () => {
    seeds.push(result.trim());
    countdown.dec();
  });
}
