'use strict';

require('../common');
const assert = require('node:assert/strict');
const childProcess = require('node:child_process');
const fixtures = require('../common/fixtures');

const child = childProcess.spawnSync(
  process.execPath,
  [ '--no-warnings', '--run'],
  { cwd: fixtures.path('run-script'), encoding: 'utf8' },
);
assert.strictEqual(child.status, 9);
console.log(child.stderr);
