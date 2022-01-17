'use strict';

const { mustCall, mustNotCall } = require('../common');
const { strictEqual } = require('assert');
const fixtures = require('../common/fixtures');
const { fork } = require('child_process');

{
  // Test aborting a forked child_process after calling fork
  const ac = new AbortController();
  const { signal } = ac;
  const cp = fork(fixtures.path('child-process-stay-alive-forever.js'), {
    signal
  });
  cp.on('exit', mustCall((code, killSignal) => {
    strictEqual(code, null);
    strictEqual(killSignal, 'SIGTERM');
  }));
  cp.on('error', mustCall((err) => {
    strictEqual(err.name, 'AbortError');
  }));
  process.nextTick(() => ac.abort());
}
{
  // Test passing an already aborted signal to a forked child_process
  const signal = AbortSignal.abort();
  const cp = fork(fixtures.path('child-process-stay-alive-forever.js'), {
    signal
  });
  cp.on('exit', mustCall((code, killSignal) => {
    strictEqual(code, null);
    strictEqual(killSignal, 'SIGTERM');
  }));
  cp.on('error', mustCall((err) => {
    strictEqual(err.name, 'AbortError');
  }));
}

{
  // Test passing a different kill signal
  const signal = AbortSignal.abort();
  const cp = fork(fixtures.path('child-process-stay-alive-forever.js'), {
    signal,
    killSignal: 'SIGKILL',
  });
  cp.on('exit', mustCall((code, killSignal) => {
    strictEqual(code, null);
    strictEqual(killSignal, 'SIGKILL');
  }));
  cp.on('error', mustCall((err) => {
    strictEqual(err.name, 'AbortError');
  }));
}

{
  // Test aborting a cp before close but after exit
  const ac = new AbortController();
  const { signal } = ac;
  const cp = fork(fixtures.path('child-process-stay-alive-forever.js'), {
    signal
  });
  cp.on('exit', mustCall(() => {
    ac.abort();
  }));
  cp.on('error', mustNotCall());

  setTimeout(() => cp.kill(), 1);
}
