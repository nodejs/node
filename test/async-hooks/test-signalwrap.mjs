import { isWindows, skip, isMainThread, mustCall } from '../common/index.mjs';

if (isWindows)
  skip('no signals in Windows');
if (!isMainThread)
  skip('No signal handling available in Workers');

import { strictEqual } from 'assert';
import initHooks from './init-hooks.mjs';
import { checkInvocations } from './hook-checks.mjs';
import { exec } from 'child_process';
import process from 'process';

const hooks = initHooks();

hooks.enable();

// Keep the event loop open so process doesn't exit before receiving signals.
const interval = setInterval(() => {}, 9999);

process.on('SIGUSR2', mustCall(onsigusr2, 2));

const as = hooks.activitiesOfTypes('SIGNALWRAP');
strictEqual(as.length, 1);
const signal1 = as[0];
strictEqual(signal1.type, 'SIGNALWRAP');
strictEqual(typeof signal1.uid, 'number');
strictEqual(typeof signal1.triggerAsyncId, 'number');
checkInvocations(signal1, { init: 1 }, 'when SIGUSR2 handler is set up');

let count = 0;
exec(`kill -USR2 ${process.pid}`);

let signal2;

function onsigusr2() {
  count++;

  if (count === 1) {
    // first invocation
    checkInvocations(
      signal1, { init: 1, before: 1 },
      ' signal1: when first SIGUSR2 handler is called for the first time');

    // Trigger same signal handler again
    exec(`kill -USR2 ${process.pid}`);
  } else {
    // second invocation
    checkInvocations(
      signal1, { init: 1, before: 2, after: 1 },
      'signal1: when first SIGUSR2 handler is called for the second time');

    // Install another signal handler
    process.removeAllListeners('SIGUSR2');
    process.on('SIGUSR2', mustCall(onsigusr2Again));

    const as = hooks.activitiesOfTypes('SIGNALWRAP');
    // The isTTY checks are needed to allow test to work whether run with
    // test.py or directly with the node executable. The third signal event
    // listener is the SIGWINCH handler that node installs when it thinks
    // process.stdout is a tty.
    const expectedLen = 2 + (!!process.stdout.isTTY || !!process.stderr.isTTY);
    strictEqual(as.length, expectedLen);
    signal2 = as[expectedLen - 1]; // Last item in the array.
    strictEqual(signal2.type, 'SIGNALWRAP');
    strictEqual(typeof signal2.uid, 'number');
    strictEqual(typeof signal2.triggerAsyncId, 'number');

    checkInvocations(
      signal1, { init: 1, before: 2, after: 1 },
      'signal1: when second SIGUSR2 handler is set up');
    checkInvocations(
      signal2, { init: 1 },
      'signal2: when second SIGUSR2 handler is setup');

    exec(`kill -USR2 ${process.pid}`);
  }
}

function onsigusr2Again() {
  clearInterval(interval);
  setImmediate(() => {
    checkInvocations(
      signal1, { init: 1, before: 2, after: 2, destroy: 1 },
      'signal1: when second SIGUSR2 handler is called');
    checkInvocations(
      signal2, { init: 1, before: 1 },
      'signal2: when second SIGUSR2 handler is called');
  });
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('SIGNALWRAP');
  checkInvocations(
    signal1, { init: 1, before: 2, after: 2, destroy: 1 },
    'signal1: when second SIGUSR2 process exits');
  // Second signal not destroyed yet since its event listener is still active
  checkInvocations(
    signal2, { init: 1, before: 1, after: 1 },
    'signal2: when second SIGUSR2 process exits');
}
