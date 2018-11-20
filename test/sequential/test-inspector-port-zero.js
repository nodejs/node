'use strict';
const { mustCall, skipIfInspectorDisabled } = require('../common');

skipIfInspectorDisabled();

const assert = require('assert');
const { URL } = require('url');
const { spawn } = require('child_process');

function test(arg, port = '') {
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
    assert(/Debugger listening on (.+)/.test(stderr));
    port = new URL(RegExp.$1).port;
    assert(+port > 0);
  });
  if (/inspect-brk/.test(arg)) {
    proc.stderr.on('data', () => {
      if (stderr.includes('\n') && !proc.killed) proc.kill();
    });
  } else {
    let onclose = () => {
      onclose = () => assert.strictEqual(port, stdout.trim());
    };
    proc.stdout.on('close', mustCall(() => onclose()));
    proc.stderr.on('close', mustCall(() => onclose()));
    proc.on('exit', mustCall((exitCode, signal) => assert.strictEqual(
      exitCode,
      0,
      `exitCode: ${exitCode}, signal: ${signal}`)));
  }
}

test('--inspect=0');
test('--inspect=127.0.0.1:0');
test('--inspect=localhost:0');

test('--inspect-brk=0');
test('--inspect-brk=127.0.0.1:0');
test('--inspect-brk=localhost:0');

// In these cases, the inspector doesn't listen, so an ephemeral port is not
// allocated and the expected value of `process.debugPort` is `0`.
test('--inspect-port=0', '0');
test('--inspect-port=127.0.0.1:0', '0');
test('--inspect-port=localhost:0', '0');
