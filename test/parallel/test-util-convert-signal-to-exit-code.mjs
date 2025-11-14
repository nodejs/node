import { mustCall, mustNotCall, isWindows } from '../common/index.mjs';
import assert from 'assert';
import { convertProcessSignalToExitCode } from 'util';
import { spawn } from 'child_process';
import os from 'os';

// Test valid signal names
{
  // SIGTERM = 15, so 128 + 15 = 143
  assert.strictEqual(convertProcessSignalToExitCode('SIGTERM'), 143);

  // SIGKILL = 9, so 128 + 9 = 137
  assert.strictEqual(convertProcessSignalToExitCode('SIGKILL'), 137);

  // SIGINT = 2, so 128 + 2 = 130
  assert.strictEqual(convertProcessSignalToExitCode('SIGINT'), 130);

  // SIGHUP = 1, so 128 + 1 = 129
  assert.strictEqual(convertProcessSignalToExitCode('SIGHUP'), 129);

  // SIGABRT = 6, so 128 + 6 = 134
  assert.strictEqual(convertProcessSignalToExitCode('SIGABRT'), 134);
}

// Test invalid signal names return null
{
  assert.strictEqual(convertProcessSignalToExitCode('INVALID'), null);
  assert.strictEqual(convertProcessSignalToExitCode(''), null);
  assert.strictEqual(convertProcessSignalToExitCode('SIG'), null);
}

// Test invalid argument types
{
  [
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
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );
  });
}

// Test with actual child process termination
{
  const cat = spawn(isWindows ? 'cmd' : 'cat');
  cat.stdout.on('end', mustCall());
  cat.stderr.on('data', mustNotCall());
  cat.stderr.on('end', mustCall());

  cat.on('exit', mustCall((code, signal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(signal, 'SIGTERM');
    assert.strictEqual(cat.signalCode, 'SIGTERM');

    // Verify the utility function returns correct exit code
    const exitCode = convertProcessSignalToExitCode(signal);
    assert.strictEqual(exitCode, 143);
  }));

  assert.strictEqual(cat.signalCode, null);
  assert.strictEqual(cat.killed, false);
  cat[Symbol.dispose]();
  assert.strictEqual(cat.killed, true);
}
