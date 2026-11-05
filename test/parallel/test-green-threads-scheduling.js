// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

const {
  GreenThread,
  kEventMessage,
  kEventError,
  kEventExit,
  kFlagTestingGlobals,
} = internalBinding('green_thread');

// A minimal promise-based wrapper around the binding, with
// __greenThreadInfo() installed in the green thread.
function spawn(source) {
  const handle = new GreenThread(source, 'test.js', 0, 0, kFlagTestingGlobals);
  const messages = [];
  const waiting = [];
  let resolveExit;
  const exited = new Promise((resolve) => { resolveExit = resolve; });
  handle.onevent = common.mustCallAtLeast((type, id, ok, value) => {
    if (type === kEventMessage) {
      if (waiting.length > 0) waiting.shift()(value);
      else messages.push(value);
    } else if (type === kEventError) {
      throw value;
    } else if (type === kEventExit) {
      handle.close();
      resolveExit(id);
    }
  }, 0);
  return {
    post(value) { handle.postMessage(value); },
    next() {
      if (messages.length > 0) return Promise.resolve(messages.shift());
      return new Promise((resolve) => waiting.push(resolve));
    },
    terminate() {
      handle.terminate();
      return exited;
    },
  };
}

// Returns a green thread that never yields on the given pool thread.
async function occupy(thread) {
  for (let i = 0; i < 1000; i++) {
    const candidate = spawn(`
      onmessage = () => {
        postMessage(__greenThreadInfo().thread);
        for (;;);
      };
    `);
    candidate.post(0);
    if (await candidate.next() === thread) return candidate;
    await candidate.terminate();
  }
  assert.fail(`could not occupy pool thread ${thread}`);
}

(async () => {
  const probe = spawn('postMessage(__greenThreadInfo());');
  const { poolSize, context, contexts } = await probe.next();
  // The initial script runs in the default context.
  assert.strictEqual(context, 1);
  assert.strictEqual(contexts, 1);
  await probe.terminate();

  // Each message gets its own invocation context, which is released when it
  // has nothing pending. Promise reactions run in the context that created
  // them.
  {
    const greenThread = spawn(`
      onmessage = () => {
        const before = __greenThreadInfo();
        Promise.resolve().then(() => {
          postMessage({ before, after: __greenThreadInfo() });
        });
      };
    `);
    greenThread.post(0);
    const first = await greenThread.next();
    greenThread.post(0);
    const second = await greenThread.next();
    assert.notStrictEqual(first.before.context, 1);
    assert.strictEqual(first.after.context, first.before.context);
    assert.notStrictEqual(second.before.context, first.before.context);
    assert.strictEqual(second.after.context, second.before.context);
    // The first context was released.
    assert.strictEqual(second.before.contexts, 2);
    await greenThread.terminate();
  }

  // Timers belong to the invocation that created them, and run on the pool
  // thread of that invocation.
  {
    const greenThread = spawn(`
      onmessage = () => {
        const created = __greenThreadInfo();
        setTimeout(() => {
          const fired = __greenThreadInfo();
          Promise.resolve().then(() => {
            postMessage({ created, fired, reaction: __greenThreadInfo() });
          });
        }, 5);
      };
    `);
    for (let i = 0; i < 10; i++) {
      greenThread.post(i);
      const { created, fired, reaction } = await greenThread.next();
      assert.strictEqual(fired.context, created.context);
      assert.strictEqual(fired.thread, created.contextThread);
      assert.strictEqual(fired.contextThread, created.contextThread);
      assert.strictEqual(reaction.context, created.context);
      assert.strictEqual(reaction.thread, created.contextThread);
    }
    await greenThread.terminate();
  }

  // A reaction whose context is still alive (because of a pending timer)
  // runs in that context. Once the context is gone, it runs in the context
  // of the task that runs it.
  {
    const greenThread = spawn(`
      let resolve;
      let timer;
      onmessage = ({ data }) => {
        const self = __greenThreadInfo().context;
        if (data === 'subscribe') {
          const promise = new Promise((r) => { resolve = r; });
          promise.then(() => postMessage({ self, reaction: __greenThreadInfo() }));
          postMessage(self);
        } else if (data === 'subscribe and keep alive') {
          timer = setTimeout(() => {}, 100000);
          const promise = new Promise((r) => { resolve = r; });
          promise.then(() => {
            const reaction = __greenThreadInfo();
            clearTimeout(timer);
            postMessage({ self, reaction, after: __greenThreadInfo() });
          });
          postMessage(self);
        } else {
          postMessage(self);
          resolve();
        }
      };
    `);

    greenThread.post('subscribe');
    await greenThread.next();
    greenThread.post('resolve');
    const resolver = await greenThread.next();
    const released = await greenThread.next();
    assert.strictEqual(released.reaction.context, resolver);

    greenThread.post('subscribe and keep alive');
    const subscriber = await greenThread.next();
    greenThread.post('resolve');
    await greenThread.next();
    const kept = await greenThread.next();
    assert.strictEqual(kept.reaction.context, subscriber);
    assert.strictEqual(kept.reaction.contexts, 3);
    // Clearing its last timer released the context.
    assert.notStrictEqual(kept.after.context, subscriber);
    assert.strictEqual(kept.after.contexts, 2);

    await greenThread.terminate();
  }

  // Timers can be cleared from another invocation, whose thread may differ
  // from the one that owns the timer's handle.
  {
    const greenThread = spawn(`
      let interval;
      let ticks = 0;
      onmessage = ({ data }) => {
        if (data === 'start') {
          interval = setInterval(() => { ticks++; }, 1);
          postMessage(__greenThreadInfo().timers);
        } else if (data === 'stop') {
          clearInterval(interval);
          const stoppedAt = ticks;
          setTimeout(() => {
            postMessage({ ticks, stoppedAt, info: __greenThreadInfo() });
          }, 20);
        }
      };
    `);
    greenThread.post('start');
    assert.strictEqual(await greenThread.next(), 1);
    await new Promise((resolve) => setTimeout(resolve, 10));
    greenThread.post('stop');
    const { ticks, stoppedAt, info } = await greenThread.next();
    assert.strictEqual(ticks, stoppedAt);
    assert.strictEqual(info.timers, 0);
    await greenThread.terminate();
  }

  if (poolSize < 2) {
    common.printSkipMessage('the remaining tests need two pool threads');
    return;
  }

  // A green thread that never yields only occupies one pool thread. The other
  // green threads keep running on the remaining threads, including ones that
  // would have been assigned to the same thread.
  {
    const busy = spawn('postMessage("started"); for (;;);');
    assert.strictEqual(await busy.next(), 'started');
    const greenThreads = Array.from({ length: 100 }, () => spawn(
      'onmessage = ({ data }) => postMessage(data + 1);'));
    for (let round = 0; round < 3; round++) {
      const results = await Promise.all(greenThreads.map((greenThread, i) => {
        greenThread.post(i);
        return greenThread.next();
      }));
      assert.deepStrictEqual(results, greenThreads.map((_, i) => i + 1));
    }
    await Promise.all(greenThreads.map((greenThread) => greenThread.terminate()));
    assert.strictEqual(await busy.terminate(), 1);
  }

  // When the thread a green thread last ran on is busy, it runs elsewhere.
  {
    const greenThread = spawn(
      'onmessage = () => postMessage(__greenThreadInfo().thread);');
    greenThread.post(0);
    const first = await greenThread.next();

    const blocker = await occupy(first);

    greenThread.post(0);
    assert.notStrictEqual(await greenThread.next(), first);
    await greenThread.terminate();
    assert.strictEqual(await blocker.terminate(), 1);
  }

  // An interval that comes due many times while its isolate is busy on
  // another thread fires once when the isolate is free, not once per missed
  // interval.
  {
    const greenThread = spawn(`
      let ticks = 0;
      onmessage = ({ data }) => {
        if (data === 'start') {
          setInterval(() => { ticks++; }, 1);
          postMessage(__greenThreadInfo().thread);
        } else if (data === 'spin') {
          postMessage(__greenThreadInfo().thread);
          const end = Date.now() + 300;
          while (Date.now() < end);
          const before = ticks;
          setTimeout(() => postMessage(ticks - before), 0);
        }
      };
    `);
    greenThread.post('start');
    const intervalThread = await greenThread.next();
    const blocker = await occupy(intervalThread);
    greenThread.post('spin');
    // It has to run elsewhere, since the blocker occupies the thread.
    assert.notStrictEqual(await greenThread.next(), intervalThread);
    // Free the interval's thread, so that it fires while the isolate spins.
    await blocker.terminate();
    const missed = await greenThread.next();
    assert(missed <= 3, `ran ${missed} queued interval callbacks`);
    await greenThread.terminate();
  }
})().then(common.mustCall());
