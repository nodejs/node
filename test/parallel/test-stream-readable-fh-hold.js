'use strict';

const { mustCall, mustNotCall } = require('../common');
const { strictEqual, notStrictEqual } = require('assert');
const fixtures = require('../common/fixtures');
const { fork } = require('child_process');

{
  const cp = fork(fixtures.path('stream-readable-leak.js'), {
    execArgv: ['--expose-gc', '--max-old-space-size=16', '--max-semi-space-size=2'],
    silent: true,
  });
  cp.on('exit', mustCall((code, killSignal) => {
    notStrictEqual(code, 1);
    strictEqual(killSignal, null);
  }));
  cp.on('error', mustNotCall());
  cp.on('message', mustCall(message => {
    strictEqual(message, `success`);
  }));
}
