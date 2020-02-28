'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

common.allowGlobals(global.require);

assert.strictEqual(
  child_process.spawnSync(process.execPath, ['console.log(42)'])
    .stdout.toString().trim(),
  '42');

assert.strictEqual(
  child_process.spawnSync(process.execPath, ['throw new Error()']).status,
  1);

assert.strictEqual(
  child_process.spawnSync(process.execPath, ['process.exitCode = 8']).status,
  8);
