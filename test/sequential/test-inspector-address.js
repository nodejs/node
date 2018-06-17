'use strict';
const { PORT, mustCall, skipIfInspectorDisabled } = require('../common');

skipIfInspectorDisabled();

const assert = require('assert');
const { spawn } = require('child_process');

function test(arg, expected, done) {
  const args = [arg, '-p', 'process.debugPort'];
  const proc = spawn(process.execPath, args);
  proc.stdout.setEncoding('utf8');
  proc.stderr.setEncoding('utf8');
  let stdout = '';
  let stderr = '';
  proc.stdout.on('data', (data) => stdout += data);
  proc.stderr.on('data', (data) => stderr += data);
  proc.stdout.on('close', (hadErr) => assert(!hadErr));
  proc.stderr.on('close', (hadErr) => assert(!hadErr));
  proc.stderr.on('data', () => {
    if (!stderr.includes('\n')) return;
    assert(/Debugger listening on (.+)\./.test(stderr));
    assert.strictEqual(RegExp.$1, expected);
  });
  proc.on('exit', (exitCode, signalCode) => {
    assert.strictEqual(exitCode, 0);
    assert.strictEqual(signalCode, null);
    done();
  });
}

function one() {
  test(`--inspect=${PORT}`, `127.0.0.1:${PORT}`, mustCall(two));
}

function two() {
  test(`--inspect=0.0.0.0:${PORT}`, `0.0.0.0:${PORT}`, mustCall(three));
}

function three() {
  test(`--inspect=127.0.0.1:${PORT}`, `127.0.0.1:${PORT}`, mustCall());
}

one();
