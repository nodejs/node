'use strict';
require('../common');

// Tests that node exits consistently on bad option syntax.

const assert = require('assert');
const spawn = require('child_process').spawnSync;

if (process.config.variables.v8_enable_inspector === 1) {
  requiresArgument('--inspect-port');
  requiresArgument('--inspect-port=');
  requiresArgument('--debug-port');
  requiresArgument('--debug-port=');
}
requiresArgument('--eval');

function requiresArgument(option) {
  const r = spawn(process.execPath, [option], { encoding: 'utf8' });

  assert.strictEqual(r.status, 9);

  const msg = r.stderr.split(/\r?\n/)[0];
  assert.strictEqual(
    msg,
    `${process.execPath}: ${option} requires an argument`
  );
}
