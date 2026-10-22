'use strict';

// https://github.com/nodejs/node/issues/61852
// Arguments following `--` are forwarded to each test file.
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const fixture = fixtures.path('test-runner', 'print-arguments.js');

{
  const args = ['--test', '--test-reporter=tap', fixture, '--', '--a-custom-argument'];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.stderr.toString(), '');
  assert.match(child.stdout.toString(), /# pass 1/);
  assert.strictEqual(child.status, 0);
}
