'use strict';

const { mustCall } = require('../common');
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
  cp.on('exit', mustCall());
  cp.on('error', mustCall((err) => {
    strictEqual(err.name, 'AbortError');
  }));
  process.nextTick(() => ac.abort());
}
{
  // Test passing an already aborted signal to a forked child_process
  const ac = new AbortController();
  const { signal } = ac;
  ac.abort();
  const cp = fork(fixtures.path('child-process-stay-alive-forever.js'), {
    signal
  });
  cp.on('exit', mustCall());
  cp.on('error', mustCall((err) => {
    strictEqual(err.name, 'AbortError');
  }));
}
