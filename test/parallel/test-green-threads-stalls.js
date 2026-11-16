// Flags: --expose-internals --experimental-green-threads --no-warnings
'use strict';

// setStallPolicy() detects green threads that keep the pool threads busy
// while other work waits for them, and breaks such stalls.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const { Worker } = require('worker_threads');
const {
  GreenThread,
  setStallPolicy,
  setUnresponsiveTimeouts,
} = require('node:green_threads');
const { internalBinding } = require('internal/test/binding');
const { kTestingFlags } = require('internal/green_threads/testing');

const {
  canCutOffThreads,
  kFlagTestingIgnoreTermination,
} = internalBinding('green_thread');

const kTimeoutMs = 200;

const spinnerSource = `onmessage = ({ data }) => {
  postMessage('spinning');
  while (Atomics.load(data, 0) === 0);
  postMessage('released');
};`;

function next(greenThread) {
  return new Promise((resolve) => greenThread.once('message', resolve));
}

// Starts `count` green threads that spin until `flag` is set, which only
// `setter` does, once it gets to run. Resolves once they all spin.
async function stall(count, flag, setter) {
  const spinners = [];
  for (let i = 0; i < count; i++) {
    spinners.push(new GreenThread(spinnerSource));
  }
  await Promise.all(spinners.map((spinner) => {
    const spinning = next(spinner);
    spinner.postMessage(flag);
    return spinning;
  }));
  setter.postMessage(flag);
  return spinners;
}

function newSetter() {
  return new GreenThread(`onmessage = ({ data }) => {
    Atomics.store(data, 0, 1);
    postMessage('set');
  };`);
}

async function terminateAll(greenThreads) {
  setStallPolicy();
  await Promise.all(greenThreads.map((greenThread) => greenThread.terminate()));
}

// Without onstall, the fallback action applies right away: the spinner
// that has run the longest is terminated, which lets the setter run.
async function fallback() {
  setStallPolicy({ timeoutMs: kTimeoutMs });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinners = await stall(4, flag, setter);
  const errors = [];
  const stalled = new Promise((resolve) => {
    for (const spinner of spinners) {
      spinner.on('error', (error) => {
        errors.push(error);
        resolve();
      });
    }
  });
  const start = Date.now();
  await set;
  await stalled;
  assert(Date.now() - start >= kTimeoutMs - 20);
  await terminateAll([setter, ...spinners]);
  assert.strictEqual(errors.length, 1);
  assert.strictEqual(errors[0].code, 'ERR_GREEN_THREAD_STALLED');
}

// The onstall function is called once, for one of them, and decides.
async function onstall() {
  const stalls = [];
  setStallPolicy({
    timeoutMs: kTimeoutMs,
    onstall: common.mustCall((stall) => {
      stalls.push(stall);
      assert.strictEqual(stall.reason, 'pool');
      assert(stall.runningMs >= kTimeoutMs);
      assert.strictEqual(stall.terminate(), true);
      // Only once.
      assert.strictEqual(stall.terminate(), false);
      assert.strictEqual(stall.extend(1000), false);
    }),
  });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinners = await stall(4, flag, setter);
  const errors = [];
  for (const spinner of spinners) {
    spinner.on('error', (error) => errors.push(error));
  }
  await set;
  assert(spinners.includes(stalls[0].greenThread));
  assert.strictEqual(await stalls[0].greenThread.terminate(), 1);
  assert.strictEqual(errors.length, 1);
  assert.strictEqual(errors[0].code, 'ERR_GREEN_THREAD_STALLED');
  assert.strictEqual(stalls[0].terminate(), false);
  await terminateAll([setter, ...spinners]);
}

// A green thread that is given more time isn't reported again for that
// time, so another one is reported next.
async function extend() {
  const reported = [];
  setStallPolicy({
    timeoutMs: kTimeoutMs,
    onstall: common.mustCall((stall) => {
      reported.push(stall.greenThread);
      if (reported.length === 1) {
        assert.strictEqual(stall.extend(60_000), true);
      } else {
        assert.strictEqual(stall.terminate(), true);
      }
    }, 2),
  });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinners = await stall(2, flag, setter);
  for (const spinner of spinners) spinner.on('error', () => {});
  await set;
  assert.notStrictEqual(reported[0], reported[1]);
  assert.strictEqual(reported[0].exitCode, undefined);
  await terminateAll([setter, ...spinners]);
}

// With extraThreads, the pool grows first, which lets the setter run.
async function extraThreads() {
  setStallPolicy({
    timeoutMs: kTimeoutMs,
    extraThreads: 1,
    onstall: common.mustNotCall(),
  });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinners = await stall(4, flag, setter);
  const released = spinners.map((spinner) => next(spinner));
  for (const spinner of spinners) spinner.on('error', common.mustNotCall());
  await set;
  await Promise.all(released);
  await terminateAll([setter, ...spinners]);
}

// Stalls caused by a GreenThread with 'stall' listeners are reported to
// them instead.
async function event() {
  setStallPolicy({ timeoutMs: kTimeoutMs, onstall: common.mustNotCall() });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinner = new GreenThread(spinnerSource);
  spinner.once('stall', common.mustCall((stall) => {
    assert.strictEqual(stall.greenThread, spinner);
    assert.strictEqual(stall.reason, 'pool');
    assert.strictEqual(stall.terminate(), true);
  }));
  spinner.on('error', common.mustCall((error) => {
    assert.strictEqual(error.code, 'ERR_GREEN_THREAD_STALLED');
  }));
  const spinning = next(spinner);
  spinner.postMessage(flag);
  await spinning;
  setter.postMessage(flag);
  await set;
  await terminateAll([setter, spinner]);
}

// Once its 'stall' listeners are removed, stalls go to onstall again.
async function listenersRemoved() {
  setStallPolicy({
    timeoutMs: kTimeoutMs,
    onstall: common.mustCall((stall) => {
      assert.strictEqual(stall.terminate(), true);
    }),
  });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinner = new GreenThread(spinnerSource);
  const listener = common.mustNotCall();
  spinner.on('stall', listener);
  spinner.prependListener('stall', listener);
  spinner.off('stall', listener);
  spinner.once('stall', listener);
  spinner.removeAllListeners('stall');
  spinner.on('error', () => {});
  const spinning = next(spinner);
  spinner.postMessage(flag);
  await spinning;
  setter.postMessage(flag);
  await set;
  await terminateAll([setter, spinner]);
}

// A green thread also stalls the timers on the pool thread it keeps busy,
// which adding threads doesn't help with.
async function timers() {
  setStallPolicy({
    timeoutMs: kTimeoutMs,
    extraThreads: 2,
    onstall: common.mustCall((stall) => {
      assert.strictEqual(stall.reason, 'thread');
      assert.strictEqual(stall.terminate(), true);
    }),
  });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  // Its timer lives on the pool thread that ran its message.
  const setter = new GreenThread(`onmessage = ({ data }) => {
    setTimeout(() => {
      Atomics.store(data, 0, 1);
      postMessage('set');
    }, 100);
    postMessage('armed');
  };`);
  const armed = next(setter);
  setter.postMessage(flag);
  await armed;
  const set = next(setter);
  const spinners = [new GreenThread(spinnerSource),
                    new GreenThread(spinnerSource)];
  for (const spinner of spinners) {
    spinner.on('error', () => {});
    spinner.postMessage(flag);
  }
  await set;
  await terminateAll([setter, ...spinners]);
}

// If the host doesn't decide within the timeout, e.g. because its event
// loop is blocked, the fallback action applies. onstall isn't called for
// stalls that have been resolved already.
async function blocked() {
  setStallPolicy({ timeoutMs: kTimeoutMs, onstall: common.mustNotCall() });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinners = await stall(2, flag, setter);
  for (const spinner of spinners) spinner.on('error', () => {});
  const start = Date.now();
  while (Atomics.load(flag, 0) === 0);
  // Detected after the timeout, and resolved after another one.
  assert(Date.now() - start >= 2 * kTimeoutMs - 40);
  await set;
  await terminateAll([setter, ...spinners]);
}

// With the 'none' fallback action, a stall that the host doesn't resolve
// is reported again after the timeout.
async function none() {
  let reported = 0;
  let flag;
  setStallPolicy({
    timeoutMs: kTimeoutMs,
    fallbackAction: 'none',
    onstall: common.mustCall(() => {
      if (++reported === 3) Atomics.store(flag, 0, 1);
    }, 3),
  });
  flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinners = await stall(2, flag, setter);
  const released = spinners.map((spinner) => next(spinner));
  for (const spinner of spinners) spinner.on('error', common.mustNotCall());
  await set;
  await Promise.all(released);
  await terminateAll([setter, ...spinners]);
}

// Without a policy, stalls are left alone.
async function removed() {
  setStallPolicy({ timeoutMs: kTimeoutMs, onstall: common.mustNotCall() });
  setStallPolicy();
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinners = await stall(2, flag, setter);
  const released = spinners.map((spinner) => next(spinner));
  for (const spinner of spinners) spinner.on('error', common.mustNotCall());
  setTimeout(() => Atomics.store(flag, 0, 1), 3 * kTimeoutMs);
  await set;
  await Promise.all(released);
  await terminateAll([setter, ...spinners]);
}

// A long task isn't a stall if nothing else waits, not even for the timers
// that the green thread itself set, which can't run before it returns anyway.
async function ownTimer() {
  setStallPolicy({ timeoutMs: kTimeoutMs, onstall: common.mustNotCall() });
  const greenThread = new GreenThread(`onmessage = () => {
    setTimeout(() => postMessage('timer'), 10);
    const start = Date.now();
    while (Date.now() - start < ${3 * kTimeoutMs});
    postMessage('done');
  };`);
  const done = next(greenThread);
  greenThread.postMessage(0);
  assert.strictEqual(await done, 'done');
  assert.strictEqual(await next(greenThread), 'timer');
  await terminateAll([greenThread]);
}

// With more spinners than pool threads, and the setter queued behind them,
// spinners are terminated, longest-running first, until the setter runs.
async function queued() {
  let terminated = 0;
  setStallPolicy({
    timeoutMs: kTimeoutMs,
    extraThreads: 1,
    onstall: common.mustCallAtLeast((stall) => {
      assert.strictEqual(stall.terminate(), true);
      terminated++;
    }),
  });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const spinners = [];
  for (let i = 0; i < 4; i++) {
    const spinner = new GreenThread(spinnerSource);
    spinner.on('error', () => {});
    spinner.postMessage(flag);
    spinners.push(spinner);
  }
  const setter = newSetter();
  const set = next(setter);
  setter.postMessage(flag);
  await set;
  // One spinner got the extra thread, and the setter took the place of the
  // second one that was terminated, or of the first one, since pool threads
  // prefer the isolates that last ran on them.
  assert(terminated === 1 || terminated === 2, `${terminated}`);
  await terminateAll([setter, ...spinners]);
}

// A green thread that ignores termination is abandoned, and its pool thread
// cut off, as for greenThread.terminate().
async function ignoresTermination() {
  setUnresponsiveTimeouts({ gracePeriodMs: kTimeoutMs });
  setStallPolicy({ timeoutMs: kTimeoutMs });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const spinner = new GreenThread(spinnerSource, {
    [kTestingFlags]: kFlagTestingIgnoreTermination,
  });
  spinner.on('error', common.mustCall((error) => {
    assert.strictEqual(error.code, 'ERR_GREEN_THREAD_STALLED');
  }));
  const spinning = next(spinner);
  spinner.postMessage(flag);
  await spinning;
  setter.postMessage(flag);
  await set;
  await terminateAll([setter, spinner]);
}

// The policy applies to green threads created by other threads too, which
// stall objects can terminate, but don't have the GreenThreads of.
async function worker() {
  setStallPolicy({
    timeoutMs: kTimeoutMs,
    onstall: common.mustCall((stall) => {
      assert.strictEqual(stall.greenThread, undefined);
      assert.strictEqual(stall.terminate(), true);
    }),
  });
  const thread = new Worker(`
    const { GreenThread } = require('node:green_threads');
    const { parentPort } = require('worker_threads');
    const flag = new Int32Array(new SharedArrayBuffer(4));
    const spinner = new GreenThread(${JSON.stringify(spinnerSource)});
    spinner.on('error', (error) => parentPort.postMessage(error.code));
    spinner.once('message', () => {
      const setter = new GreenThread(\`onmessage = ({ data }) => {
        Atomics.store(data, 0, 1);
        postMessage('set');
      };\`);
      setter.once('message', (value) => {
        parentPort.postMessage(value);
        process.exit();
      });
      setter.postMessage(flag);
    });
    spinner.postMessage(flag);
  `, { eval: true });
  const messages = [];
  thread.on('message', (message) => messages.push(message));
  await new Promise((resolve) => thread.on('exit', resolve));
  assert.deepStrictEqual(messages.sort(), ['ERR_GREEN_THREAD_STALLED', 'set']);
  setStallPolicy();
}

// Changing the policy from onstall doesn't affect the stall that it was
// called for: its fallback action still applies.
async function reentrant() {
  setStallPolicy({
    timeoutMs: kTimeoutMs,
    onstall: common.mustCall(() => setStallPolicy()),
  });
  const flag = new Int32Array(new SharedArrayBuffer(4));
  const setter = newSetter();
  const set = next(setter);
  const [spinner] = await stall(1, flag, setter);
  spinner.on('error', common.mustCall((error) => {
    assert.strictEqual(error.code, 'ERR_GREEN_THREAD_STALLED');
  }));
  await set;
  await terminateAll([setter, spinner]);
}

const cases = {
  fallback,
  onstall,
  extend,
  extraThreads,
  event,
  listenersRemoved,
  timers,
  blocked,
  none,
  removed,
  ownTimer,
  queued,
  worker,
  reentrant,
};
if (canCutOffThreads) cases.ignoresTermination = ignoresTermination;

if (process.argv[2] === 'child') {
  cases[process.argv[3]]().then(common.mustCall());
  return;
}

for (const [policy, code] of [
  [null, 'ERR_INVALID_ARG_TYPE'],
  [{}, 'ERR_INVALID_ARG_TYPE'],
  [{ timeoutMs: 0 }, 'ERR_OUT_OF_RANGE'],
  [{ timeoutMs: 1.5 }, 'ERR_OUT_OF_RANGE'],
  [{ timeoutMs: 100, extraThreads: -1 }, 'ERR_OUT_OF_RANGE'],
  [{ timeoutMs: 100, fallbackAction: 'abort' }, 'ERR_INVALID_ARG_VALUE'],
  [{ timeoutMs: 100, onstall: 'yes' }, 'ERR_INVALID_ARG_TYPE'],
]) {
  assert.throws(() => setStallPolicy(policy), { code });
}

// The pool is process-wide, and grows with extraThreads, so each case runs
// in its own process, with a pool of as many threads as it has spinners.
const poolSizes = {
  fallback: 4,
  onstall: 4,
  extend: 2,
  extraThreads: 4,
  event: 1,
  listenersRemoved: 1,
  timers: 2,
  blocked: 2,
  none: 2,
  removed: 2,
  ownTimer: 1,
  queued: 2,
  ignoresTermination: 1,
  worker: 1,
  reentrant: 1,
};
for (const name of Object.keys(cases)) {
  spawnSyncAndExitWithoutError(process.execPath, [
    '--expose-internals', '--experimental-green-threads', '--no-warnings',
    __filename, 'child', name,
  ], {
    env: { ...process.env, NODE_GREEN_THREADS_POOL_SIZE: poolSizes[name] },
  });
}
