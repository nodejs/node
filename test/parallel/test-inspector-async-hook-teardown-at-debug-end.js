'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();

const spawn = require('child_process').spawn;

const script = `
const assert = require('assert');

// Verify that inspector-async-hook is registered
// by checking that emitInit with invalid arguments
// throw an error.
// See test/async-hooks/test-emit-init.js
assert.throws(
  () => async_hooks.emitInit(),
  'inspector async hook should have been enabled initially');

process._debugEnd();

// Verify that inspector-async-hook is no longer registered,
// thus emitInit() ignores invalid arguments
// See test/async-hooks/test-emit-init.js
assert.doesNotThrow(
  () => async_hooks.emitInit(),
  'inspector async hook should have beend disabled by _debugEnd()');
`;

const args = ['--inspect', '-e', script];
const child = spawn(process.execPath, args, { stdio: 'inherit' });
child.on('exit', (code, signal) => {
  process.exit(code || signal);
});
