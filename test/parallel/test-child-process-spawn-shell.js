'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
{
  // Verify that a shell is, in fact, executed
  const doesNotExist = cp.spawn('does-not-exist', { shell: true });

  assert.notStrictEqual(doesNotExist.spawnfile, 'does-not-exist');
  doesNotExist.on('error', common.mustNotCall());
  doesNotExist.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(signal, null);

    if (common.isWindows)
      assert.strictEqual(code, 1);    // Exit code of cmd.exe
    else
      assert.strictEqual(code, 127);  // Exit code of /bin/sh
  }));
}

{
  // Verify that passing arguments works
  const echo = cp.spawn('echo', ['foo'], {
    encoding: 'utf8',
    shell: true
  });
  let echoOutput = '';

  assert.strictEqual(echo.spawnargs[echo.spawnargs.length - 1].replace(/"/g, ''),
                     'echo foo');
  echo.stdout.on('data', (data) => {
    echoOutput += data;
  });
  echo.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(echoOutput.trim(), 'foo');
  }));
}

{
  // Verify that shell features can be used
  const cmd = 'echo bar | cat';
  const command = cp.spawn(cmd, {
    encoding: 'utf8',
    shell: true
  });
  let commandOutput = '';

  command.stdout.on('data', (data) => {
    commandOutput += data;
  });
  command.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(commandOutput.trim(), 'bar');
  }));
}

{
  // Verify that shell features can not be passed through
  // object prototype manipulation, and must be strictly
  // passed through the options object
  const spawnCmd = 'date';
  const spawnCmdArgs = ['; echo baz'];
  Object.prototype.shell = true;

  const spawnHandler = cp.spawn(spawnCmd, spawnCmdArgs, {
    encoding: 'utf8'
  });

  // assertion is expected to fail due to `spawnCmdArgs` not
  // being a valid argument to the date command
  const SUCCESS_CODE = 0;
  spawnHandler.on('close', common.mustCall((code) => {
    assert.notStrictEqual(code, SUCCESS_CODE);
  }));

  Reflect.deleteProperty(Object.prototype, 'shell');
}

{
  // Verify that the environment is properly inherited
  const env = cp.spawn(`"${process.execPath}" -pe process.env.BAZ`, {
    env: Object.assign({}, process.env, { BAZ: 'buzz' }),
    encoding: 'utf8',
    shell: true
  });
  let envOutput = '';

  env.stdout.on('data', (data) => {
    envOutput += data;
  });
  env.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(envOutput.trim(), 'buzz');
  }));
}
