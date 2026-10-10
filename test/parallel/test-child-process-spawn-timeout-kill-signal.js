'use strict';

const common = require('../common');
const { mustCall } = common;
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const { listenerCount } = require('events');

const aliveForeverFile = 'child-process-stay-alive-forever.js';
{
  // Verify default signal + closes
  const cp = spawn(process.execPath, [fixtures.path(aliveForeverFile)], {
    timeout: 5,
  });
  assert.strictEqual(cp.timedOut, false);
  cp.on('exit', mustCall((code, ks) => {
    assert.strictEqual(ks, 'SIGTERM');
    assert.strictEqual(cp.timedOut, true);
  }));
}

{
  // Verify timedOut stays false when the child exits before the timeout
  const cp = spawn(process.execPath, ['-e', ''], { timeout: 2 ** 30 });
  cp.on('exit', mustCall((code) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(cp.timedOut, false);
  }));
}

{
  // Verify timedOut stays false when the child exits in time but a descendant
  // keeps its stdio open past the timeout, so that the timer fires after exit.
  const cp = spawn(process.execPath, ['-e', `
    const { spawn } = require('child_process');
    spawn(process.execPath, ['-e', 'setTimeout(() => {}, ${common.platformTimeout(1500)})'],
          { stdio: 'inherit' }).unref();
  `], { timeout: common.platformTimeout(500) });
  cp.on('exit', mustCall((code) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(cp.timedOut, false);
  }));
  cp.on('close', mustCall(() => assert.strictEqual(cp.timedOut, false)));
}

{
  // Verify timedOut stays false when the child is killed by the caller
  const cp = spawn(process.execPath, [fixtures.path(aliveForeverFile)], {
    timeout: 2 ** 30,
  });
  cp.on('spawn', mustCall(() => cp.kill()));
  cp.on('exit', mustCall(() => {
    assert.strictEqual(cp.killed, true);
    assert.strictEqual(cp.timedOut, false);
  }));
}

{
  // Verify SIGKILL signal + closes
  const cp = spawn(process.execPath, [fixtures.path(aliveForeverFile)], {
    timeout: 6,
    killSignal: 'SIGKILL',
  });
  cp.on('exit', mustCall((code, ks) => assert.strictEqual(ks, 'SIGKILL')));
}

{
  // Verify timeout verification
  assert.throws(() => spawn(process.execPath, [fixtures.path(aliveForeverFile)], {
    timeout: 'badValue',
  }), /ERR_INVALID_ARG_TYPE/);

  assert.throws(() => spawn(process.execPath, [fixtures.path(aliveForeverFile)], {
    timeout: {},
  }), /ERR_INVALID_ARG_TYPE/);
}

{
  // Verify abort signal gets unregistered
  const controller = new AbortController();
  const { signal } = controller;
  const cp = spawn(process.execPath, [fixtures.path(aliveForeverFile)], {
    timeout: 6,
    signal,
  });
  assert.strictEqual(listenerCount(signal, 'abort'), 1);
  cp.on('exit', mustCall(() => {
    assert.strictEqual(listenerCount(signal, 'abort'), 0);
  }));
}
