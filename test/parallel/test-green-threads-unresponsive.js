// Flags: --expose-internals --experimental-green-threads --no-warnings
'use strict';

// Green threads that don't stop when asked to, e.g. because V8 compiled the
// running code in a way that doesn't handle interrupts, are abandoned after
// a grace period, and the pool thread running them is cut off. If that isn't
// possible, the process is aborted.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const { internalBinding } = require('internal/test/binding');
const { GreenThread } = require('node:green_threads');

const binding = internalBinding('green_thread');
const {
  kEventMessage,
  kEventError,
  kEventExit,
  kEventAbandoned,
  kErrorCpuLimit,
  kFlagTestingGlobals,
  kFlagTestingIgnoreTermination,
} = binding;

const kGracePeriodMs = 100;

// A green thread stuck in C++, where it can't be cut off, aborts the process.
// This works everywhere.
if (process.argv[2] === 'block') {
  binding.setUnresponsiveTimeoutsForTesting(kGracePeriodMs, 200);
  const handle = new binding.GreenThread(
    'postMessage(0); __greenThreadBlock();', 'test.js', 0, 0,
    kFlagTestingGlobals);
  handle.onevent = (type) => {
    if (type === kEventMessage) handle.terminate();
  };
  return;
}
{
  const child = require('child_process').spawnSync(process.execPath, [
    '--expose-internals', '--experimental-green-threads', '--no-warnings',
    __filename, 'block',
  ]);
  assert.strictEqual(child.signal, common.isWindows ? null : 'SIGABRT');
  assert.match(child.stderr.toString(),
               /A green thread ignored termination, and the thread running it could not be stopped/);
}

if (!binding.canCutOffThreads) {
  common.printSkipMessage('Cutting off threads is not supported here');
  return;
}

binding.setUnresponsiveTimeoutsForTesting(kGracePeriodMs, 30000);

// Resolves with the events of a green thread that ignores termination. Its
// source posts a message once it is about to get stuck, and it's terminated
// then.
function spawnStuck(source, { maxTaskCpuTimeMs = 0, terminate = true } = {}) {
  const handle = new binding.GreenThread(
    source, 'test.js', 0, 0, kFlagTestingIgnoreTermination, 0, 0,
    maxTaskCpuTimeMs);
  const events = [];
  let start;
  return new Promise((resolve) => {
    handle.onevent = common.mustCallAtLeast((type, id) => {
      if (type === kEventMessage) {
        start = performance.now();
        if (terminate) handle.terminate();
        return;
      }
      events.push([type, id]);
      if (type === kEventExit) {
        handle.close();
        resolve({ events, elapsed: performance.now() - start });
      }
    });
  });
}

async function checkPool() {
  // All pool threads still work.
  const greenThreads = Array.from({ length: 32 }, () => new GreenThread(
    'onmessage = ({ data }) => postMessage(data * 2);'));
  greenThreads.forEach((g, i) => g.postMessage(i));
  const replies = await Promise.all(greenThreads.map((g) => {
    return new Promise((resolve) => g.once('message', resolve));
  }));
  assert.deepStrictEqual(replies, greenThreads.map((g, i) => i * 2));
  await Promise.all(greenThreads.map((g) => g.terminate()));
}

(async () => {
  // terminate() doesn't stop it, so it's abandoned after the grace period.
  {
    const { events, elapsed } = await spawnStuck('postMessage(0); for (;;);');
    assert.deepStrictEqual(events, [[kEventAbandoned, kGracePeriodMs],
                                    [kEventExit, 1]]);
    assert(elapsed >= kGracePeriodMs, `${elapsed}`);
  }
  await checkPool();

  // The CPU time limit doesn't stop it either. The host still learns why it
  // was stopped.
  {
    const { events } = await spawnStuck('postMessage(0); for (;;);', {
      maxTaskCpuTimeMs: 10,
      terminate: false,
    });
    assert.deepStrictEqual(events, [[kEventError, kErrorCpuLimit],
                                    [kEventAbandoned, kGracePeriodMs],
                                    [kEventExit, 1]]);
  }

  // As many stuck green threads as there are pool threads, in timer
  // callbacks and microtasks too, which all get cut off.
  await Promise.all([
    'postMessage(0); for (;;);',
    'setTimeout(() => { postMessage(0); for (;;); }, 1);',
    'Promise.resolve().then(() => { postMessage(0); for (;;); });',
    'function f() { postMessage(0); for (;;); } f();',
  ].flatMap((source) => [spawnStuck(source), spawnStuck(source)]));
  await checkPool();

  // A green thread that is cut off doesn't keep the process from exiting
  // normally, nor does one that is abandoned while the process exits.
  for (const exit of ['', 'process.exit(3);']) {
    spawnSyncAndAssert(process.execPath, [
      '--expose-internals', '--experimental-green-threads', '--no-warnings',
      '-e',
      `
      const { internalBinding } = require('internal/test/binding');
      const binding = internalBinding('green_thread');
      binding.setUnresponsiveTimeoutsForTesting(${kGracePeriodMs}, 30000);
      const handle = new binding.GreenThread(
        'postMessage(0); for (;;);', 'test.js', 0, 0,
        binding.kFlagTestingIgnoreTermination);
      handle.onevent = (type) => {
        if (type !== binding.kEventMessage) return;
        handle.unref();
        handle.terminate();
        ${exit}
      };
      `,
    ], { status: exit ? 3 : 0, stdout: '', stderr: '' });
  }
})().then(common.mustCall());
