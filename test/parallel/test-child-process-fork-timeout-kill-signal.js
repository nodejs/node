'use strict';

const { mustCall } = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { fork } = require('child_process');
const { listenerCount } = require('events');

{
  // Verify default signal
  const cp = fork(fixtures.path('child-process-stay-alive-forever.js'), {
    timeout: 5,
  });
  cp.on('exit', mustCall((code, ks) => assert.strictEqual(ks, 'SIGTERM')));
}

{
  // Verify correct signal + closes after at least 4 ms.
  const cp = fork(fixtures.path('child-process-stay-alive-forever.js'), {
    timeout: 5,
    killSignal: 'SIGKILL',
  });
  cp.on('exit', mustCall((code, ks) => assert.strictEqual(ks, 'SIGKILL')));
}

{
  // Verify timeout verification
  assert.throws(() => fork(fixtures.path('child-process-stay-alive-forever.js'), {
    timeout: 'badValue',
  }), /ERR_INVALID_ARG_TYPE/);

  assert.throws(() => fork(fixtures.path('child-process-stay-alive-forever.js'), {
    timeout: {},
  }), /ERR_INVALID_ARG_TYPE/);
}

{
  // Verify abort signal gets unregistered
  const signal = new EventTarget();
  signal.aborted = false;

  const cp = fork(fixtures.path('child-process-stay-alive-forever.js'), {
    timeout: 6,
    signal,
  });
  assert.strictEqual(listenerCount(signal, 'abort'), 1);
  cp.on('exit', mustCall(() => {
    assert.strictEqual(listenerCount(signal, 'abort'), 0);
  }));
}
