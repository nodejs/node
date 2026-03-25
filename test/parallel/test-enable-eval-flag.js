'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

const message = /Code generation from strings disallowed for this context/;

// Check default behavior (blocked)
// We test this in a subprocess to ensure a clean state
const blockedChild = spawnSync(process.execPath, [
  '-e',
  'eval("1")'
]);
assert.notStrictEqual(blockedChild.status, 0);
assert.match(blockedChild.stderr.toString(), message);

// Check --enable-eval behavior (allowed)
const allowedChild = spawnSync(process.execPath, [
  '--enable-eval',
  '-e',
  'console.log(eval("1 + 1")); console.log(new Function("return 2")())'
]);
assert.strictEqual(allowedChild.status, 0);
assert.strictEqual(allowedChild.stdout.toString().trim(), '2\n2');

// Check behavior within the current process (should be blocked by default)
assert.throws(() => eval('1'), {
  name: 'EvalError',
  message: message
});

assert.throws(() => new Function('return 1'), {
  name: 'EvalError',
  message: message
});

console.log('All tests passed');
