'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');

common.allowGlobals(global.require);
common.allowGlobals(global.embedVars);
let binary = `out/${common.buildType}/embedtest`;
if (common.isWindows) {
  binary += '.exe';
}
binary = path.resolve(__dirname, '..', '..', binary);

const env = {
  ...process.env,
  NODE_DISABLE_COLORS: true
};
const spawnOptions = {
  env,
  encoding: 'utf8'
};

assert.strictEqual(
  child_process.spawnSync(binary, ['console.log(42)'], spawnOptions)
    .stdout.trim(),
  '42');

assert.strictEqual(
  child_process.spawnSync(binary, ['console.log(embedVars.nön_ascıı)'], spawnOptions)
    .stdout.trim(),
  '🏳️‍🌈');

assert.strictEqual(
  child_process.spawnSync(binary, ['console.log(42)'], spawnOptions)
    .stdout.trim(),
  '42');

assert.strictEqual(
  child_process.spawnSync(binary, ['throw new Error()'], spawnOptions).status,
  1);

assert.strictEqual(
  child_process.spawnSync(binary, ['process.exitCode = 8'], spawnOptions).status,
  8);


const fixturePath = JSON.stringify(fixtures.path('exit.js'));
assert.strictEqual(
  child_process.spawnSync(binary, [`require(${fixturePath})`, 92], spawnOptions).status,
  92);
