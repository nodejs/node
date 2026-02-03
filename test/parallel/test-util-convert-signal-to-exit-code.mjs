import { mustCall, mustNotCall, isWindows } from '../common/index.mjs';
import assert from 'assert';
import { convertProcessSignalToExitCode } from 'util';
import { spawn } from 'child_process';
import { constants } from 'os';
const { signals } = constants;

{

  assert.strictEqual(convertProcessSignalToExitCode('SIGTERM'), 128 + signals.SIGTERM);
  assert.strictEqual(convertProcessSignalToExitCode('SIGKILL'), 128 + signals.SIGKILL);
  assert.strictEqual(convertProcessSignalToExitCode('SIGINT'), 128 + signals.SIGINT);
  assert.strictEqual(convertProcessSignalToExitCode('SIGHUP'), 128 + signals.SIGHUP);
  assert.strictEqual(convertProcessSignalToExitCode('SIGABRT'), 128 + signals.SIGABRT);
}

{
  [
    'INVALID',
    '',
    'SIG',
    undefined,
    null,
    123,
    true,
    false,
    {},
    [],
    Symbol('test'),
    () => {},
  ].forEach((value) => {
    assert.throws(
      () => convertProcessSignalToExitCode(value),
      {
        code: 'ERR_INVALID_ARG_VALUE',
        name: 'TypeError',
      }
    );
  });
}

{
  const cat = spawn(isWindows ? 'cmd' : 'cat');
  cat.stdout.on('end', mustCall());
  cat.stderr.on('data', mustNotCall());
  cat.stderr.on('end', mustCall());

  cat.on('exit', mustCall((code, signal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(signal, 'SIGTERM');
    assert.strictEqual(cat.signalCode, 'SIGTERM');

    const exitCode = convertProcessSignalToExitCode(signal);
    assert.strictEqual(exitCode, 143);
  }));

  assert.strictEqual(cat.signalCode, null);
  assert.strictEqual(cat.killed, false);
  cat[Symbol.dispose]();
  assert.strictEqual(cat.killed, true);
}
