'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const spawn = require('child_process').spawn;

const script = `
const assert = require('assert');
const inspector = process.binding('inspector');

assert(
  !!inspector.isEnabled(),
  'inspector.isEnabled() should be true when run with --inspect');

process._debugEnd();

assert(
  !inspector.isEnabled(),
  'inspector.isEnabled() should be false after _debugEnd()');
`;

const args = ['--inspect', '-e', script];
const child = spawn(process.execPath, args, {
  stdio: 'inherit',
  env: { ...process.env, NODE_V8_COVERAGE: '' }
});
child.on('exit', (code, signal) => {
  process.exit(code || signal);
});
