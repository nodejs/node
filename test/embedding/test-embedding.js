'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');

common.allowGlobals(global.require);
let binary = `out/${common.buildType}/embedtest`;
if (common.isWindows) {
  binary += '.exe';
}
binary = path.resolve(__dirname, '..', '..', binary);

assert.strictEqual(
  child_process.spawnSync(binary, ['console.log(42)'])
    .stdout.toString().trim(),
  '42');

assert.strictEqual(
  child_process.spawnSync(binary, ['throw new Error()']).status,
  1);

assert.strictEqual(
  child_process.spawnSync(binary, ['process.exitCode = 8']).status,
  8);


const fixturePath = JSON.stringify(fixtures.path('exit.js'));
assert.strictEqual(
  child_process.spawnSync(binary, [`require(${fixturePath})`, 92]).status,
  92);
