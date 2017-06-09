'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

// Verify that a shell is, in fact, executed
const doesNotExist = cp.spawnSync('does-not-exist', {shell: true});

assert.notStrictEqual(doesNotExist.file, 'does-not-exist');
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
const cmd = 'echo bar | cat';
const command = cp.spawnSync(cmd, {shell: true});

assert.strictEqual(command.stdout.toString().trim(), 'bar');

// Verify that the environment is properly inherited
const env = cp.spawnSync(`"${process.execPath}" -pe process.env.BAZ`, {
  env: Object.assign({}, process.env, {BAZ: 'buzz'}),
  shell: true
});

assert.strictEqual(env.stdout.toString().trim(), 'buzz');

// Verify that the shell internals work properly across platforms.
{
  const originalComspec = process.env.comspec;

  // Enable monkey patching process.platform.
  const originalPlatform = process.platform;
  let platform = null;
  Object.defineProperty(process, 'platform', { get: () => platform });

  function test(testPlatform, shell, shellOutput) {
    platform = testPlatform;

    const cmd = 'not_a_real_command';
    const shellFlags = platform === 'win32' ? ['/d', '/s', '/c'] : ['-c'];
    const outputCmd = platform === 'win32' ? `"${cmd}"` : cmd;
    const windowsVerbatim = platform === 'win32' ? true : undefined;
    const result = cp.spawnSync(cmd, { shell });

    assert.strictEqual(result.file, shellOutput);
    assert.deepStrictEqual(result.args,
                           [shellOutput, ...shellFlags, outputCmd]);
    assert.strictEqual(result.options.shell, shell);
    assert.strictEqual(result.options.file, result.file);
    assert.deepStrictEqual(result.options.args, result.args);
    assert.strictEqual(result.options.windowsVerbatimArguments,
                       windowsVerbatim);
  }

  // Test Unix platforms with the default shell.
  test('darwin', true, '/bin/sh');

  // Test Unix platforms with a user specified shell.
  test('darwin', '/bin/csh', '/bin/csh');

  // Test Android platforms.
  test('android', true, '/system/bin/sh');

  // Test Windows platforms with a user specified shell.
  test('win32', 'powershell.exe', 'powershell.exe');

  // Test Windows platforms with the default shell and no comspec.
  delete process.env.comspec;
  test('win32', true, 'cmd.exe');

  // Test Windows platforms with the default shell and a comspec value.
  process.env.comspec = 'powershell.exe';
  test('win32', true, process.env.comspec);

  // Restore the original value of process.platform.
  platform = originalPlatform;

  // Restore the original comspec environment variable if necessary.
  if (originalComspec)
    process.env.comspec = originalComspec;
}
