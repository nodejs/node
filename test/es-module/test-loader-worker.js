'use strict';

require('../common');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { execPath } = require('process');
const { writeFileSync } = require('fs');
const { join } = require('path');

tmpdir.refresh();
writeFileSync(join(tmpdir.path, 'hello.cjs'), 'console.log("hello")', 'utf8');
writeFileSync(join(tmpdir.path, 'loader.mjs'), 'import "./hello.cjs"', 'utf8');
writeFileSync(join(tmpdir.path, 'main.cjs'), 'setTimeout(() => {}, 1000)', 'utf8');

const child = spawnSync(execPath, ['--experimental-loader', './loader.mjs', 'main.cjs'], {
  cwd: tmpdir.path
});

const stdout = child.stdout.toString().trim();
const stderr = child.stderr.toString().trim();
const hellos = [...stdout.matchAll(/hello/g)];
const warnings = [...stderr.matchAll(/ExperimentalWarning: Custom ESM Loaders/g)];

assert.strictEqual(child.status, 0);
assert.strictEqual(hellos.length, 1);
assert.strictEqual(warnings.length, 1);
