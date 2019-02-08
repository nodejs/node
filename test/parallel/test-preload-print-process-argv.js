'use strict';

// This tests that process.argv is the same in the preloaded module
// and the user module.

const common = require('../common');

const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');

if (!common.isMainThread) {
  common.skip('Cannot chdir to the tmp directory in workers');
}

tmpdir.refresh();

process.chdir(tmpdir.path);
fs.writeFileSync(
  'preload.js',
  'console.log(JSON.stringify(process.argv));',
  'utf-8');

fs.writeFileSync(
  'main.js',
  'console.log(JSON.stringify(process.argv));',
  'utf-8');

const child = spawnSync(process.execPath, ['-r', './preload.js', 'main.js']);

if (child.status !== 0) {
  console.log(child.stderr.toString());
  assert.strictEqual(child.status, 0);
}

const lines = child.stdout.toString().trim().split('\n');
assert.deepStrictEqual(JSON.parse(lines[0]), JSON.parse(lines[1]));
