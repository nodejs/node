'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

// Verify that a shell is, in fact, executed
const doesNotExist = cp.spawnSync('does-not-exist', {shell: true});

assert.notEqual(doesNotExist.file, 'does-not-exist');
assert.strictEqual(doesNotExist.error, undefined);
assert.strictEqual(doesNotExist.signal, null);

if (common.isWindows)
  assert.strictEqual(doesNotExist.status, 1);    // Exit code of cmd.exe
else
  assert.strictEqual(doesNotExist.status, 127);  // Exit code of /bin/sh

// Verify that passing arguments works
const echo = cp.spawnSync('echo', ['foo'], {shell: true});

assert.strictEqual(echo.args[echo.args.length - 1].replace(/"/g, ''),
                   'echo foo');
assert.strictEqual(echo.stdout.toString().trim(), 'foo');

// Verify that shell features can be used
const cmd = common.isWindows ? 'echo bar | more' : 'echo bar | cat';
const command = cp.spawnSync(cmd, {shell: true});

assert.strictEqual(command.stdout.toString().trim(), 'bar');

// Verify that the environment is properly inherited
const env = cp.spawnSync(`"${process.execPath}" -pe process.env.BAZ`, {
  env: Object.assign({}, process.env, {BAZ: 'buzz'}),
  shell: true
});

assert.strictEqual(env.stdout.toString().trim(), 'buzz');
