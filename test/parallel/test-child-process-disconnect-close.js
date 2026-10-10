'use strict';
// Regression test for https://github.com/nodejs/node/issues/19433:
// after the parent explicitly calls subprocess.disconnect(), the subprocess
// must still emit 'close' (exactly once) when it exits and its stdio closes,
// regardless of whether it exits on its own or is killed.
const common = require('../common');
const assert = require('assert');
const { fork } = require('child_process');

if (process.argv[2] === 'child') {
  const mode = process.argv[3];
  // Keep the event loop alive until the parent disconnects (or kills us).
  const timer = setInterval(() => {}, 1000);
  process.on('disconnect', () => {
    clearInterval(timer);
    if (mode === 'self-exit')
      process.exit(42);
  });
  process.send('ready');
  return;
}

function test(mode, expectedCode, expectedSignal) {
  const child = fork(__filename, ['child', mode]);
  const events = [];

  child.on('disconnect', common.mustCall(() => {
    events.push('disconnect');
    assert.strictEqual(child.connected, false);
    assert.strictEqual(child.channel, null);
  }));

  child.on('exit', common.mustCall((code, signal) => {
    events.push('exit');
    assert.strictEqual(code, expectedCode);
    assert.strictEqual(signal, expectedSignal);
  }));

  child.on('close', common.mustCall((code, signal) => {
    events.push('close');
    assert.strictEqual(code, expectedCode);
    assert.strictEqual(signal, expectedSignal);
    assert.deepStrictEqual(events, ['disconnect', 'exit', 'close']);
  }));

  child.once('message', common.mustCall((message) => {
    assert.strictEqual(message, 'ready');
    child.disconnect();
    if (mode === 'kill')
      child.kill('SIGKILL');
  }));
}

test('self-exit', 42, null);
test('kill', null, 'SIGKILL');
