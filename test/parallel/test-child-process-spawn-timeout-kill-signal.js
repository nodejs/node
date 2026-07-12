'use strict';

const { mustCall } = require('../common');
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
  cp.on('exit', mustCall((code, ks) => assert.strictEqual(ks, 'SIGTERM')));
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
