'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');

common.allowGlobals(global.require);
common.allowGlobals(global.embedVars);
common.allowGlobals(global.import);
common.allowGlobals(global.module);
let binary = `out/${common.buildType}/napi_embedding`;
if (common.isWindows) {
  binary += '.exe';
}
binary = path.resolve(__dirname, '..', '..', binary);

assert.strictEqual(
  child_process.spawnSync(binary, ['console.log(42)'])
    .stdout.toString().trim(),
  '42');

assert.strictEqual(
  child_process.spawnSync(binary, ['console.log(embedVars.nön_ascıı)'])
    .stdout.toString().trim(),
  '🏳️‍🌈');

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

assert.strictEqual(
  child_process.spawnSync(binary, ['function callMe(text) { return text + " you"; }'])
    .stdout.toString().trim(),
  'called you');

assert.strictEqual(
  child_process.spawnSync(binary, ['function waitMe(text, cb) { setTimeout(() => cb(text + " you"), 1); }'])
    .stdout.toString().trim(),
  'waited you');

assert.strictEqual(
  child_process.spawnSync(binary,
                          ['function waitPromise(text)' +
      '{ return new Promise((res) => setTimeout(() => res(text + " with cheese"), 1)); }'])
    .stdout.toString().trim(),
  'waited with cheese');

assert.strictEqual(
  child_process.spawnSync(binary,
                          ['function waitPromise(text)' +
      '{ return new Promise((res, rej) => setTimeout(() => rej(text + " without cheese"), 1)); }'])
    .stdout.toString().trim(),
  'waited without cheese');

assert.match(
  child_process.spawnSync(binary,
                          ['0syntax_error'])
    .stderr.toString().trim(),
  /SyntaxError: Invalid or unexpected token/);
