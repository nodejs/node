'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

// This test ensures that the --debug-brk flag will exit the process
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { spawnSync } = require('child_process');

// File name here doesn't actually matter the process will exit on start.
const script = fixtures.path('empty.js');

function test(arg) {
  const child = spawnSync(process.execPath, ['--inspect', arg, script]);
  const stderr = child.stderr.toString();
  assert(stderr.includes('DEP0062'));
  assert.strictEqual(child.status, 9);
}

test('--debug-brk');
test('--debug-brk=5959');
