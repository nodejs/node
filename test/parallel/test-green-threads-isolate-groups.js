// Flags: --expose-internals --experimental-green-threads --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { internalBinding } = require('internal/test/binding');
const { GreenThread, IsolateGroup } = require('node:green_threads');

const binding = internalBinding('green_thread');
const {
  kEventMessage,
  kEventError,
  kEventExit,
  kFlagTestingGlobals,
  kIsolateGroupDefault,
  kIsolateGroupNew,
  kIsolateGroupShared,
} = binding;

assert.strictEqual(typeof IsolateGroup.supported, 'boolean');
assert.strictEqual(IsolateGroup.supported, binding.isolateGroupsSupported);

// Resolves with what __greenThreadInfo() returns in a green thread created in
// the given group.
function info(groupKind, groupHandle) {
  const handle = new binding.GreenThread(
    'postMessage(__greenThreadInfo()); close();', 'test.js', 0, 0,
    kFlagTestingGlobals, 0, 0, 0, groupKind, groupHandle);
  return new Promise((resolve, reject) => {
    let result;
    handle.onevent = (type, id, ok, value) => {
      if (type === kEventMessage) {
        result = value;
      } else if (type === kEventError) {
        reject(value);
      } else if (type === kEventExit) {
        handle.close();
        resolve(result);
      }
    };
  });
}

assert.throws(() => new GreenThread('', { isolateGroup: 'shared' }),
              { code: 'ERR_INVALID_ARG_VALUE' });
assert.throws(() => new GreenThread('', { isolateGroup: {} }),
              { code: 'ERR_INVALID_ARG_VALUE' });

(async () => {
  // V8's default group is always available, and uses the read-only snapshot
  // of Node.js' own.
  {
    const { isolateGroup, usesNodeSnapshot } = await info(kIsolateGroupDefault);
    assert.strictEqual(isolateGroup, 0);
    assert.strictEqual(usesNodeSnapshot, true);
    const greenThread = new GreenThread('', { isolateGroup: 'default' });
    assert.strictEqual(await greenThread.evaluate('6 * 7'), 42);
    await greenThread.terminate();
  }

  if (!IsolateGroup.supported) {
    assert.throws(() => new IsolateGroup(),
                  { code: 'ERR_GREEN_THREAD_ISOLATE_GROUPS_UNSUPPORTED' });
    assert.throws(() => new GreenThread('', { isolateGroup: 'new' }),
                  { code: 'ERR_GREEN_THREAD_ISOLATE_GROUPS_UNSUPPORTED' });
    // By default, green threads are in the default group.
    assert.strictEqual((await info()).isolateGroup, 0);
    return;
  }

  // By default, and with 'new', each green thread gets a group of its own,
  // and uses V8's own read-only snapshot.
  {
    const infos = await Promise.all([
      info(), info(), info(kIsolateGroupNew), info(kIsolateGroupNew),
    ]);
    const groups = new Set(infos.map((i) => i.isolateGroup));
    assert.strictEqual(groups.size, infos.length);
    assert(!groups.has(0));
    for (const { usesNodeSnapshot } of infos) {
      assert.strictEqual(usesNodeSnapshot, false);
    }
  }

  // Green threads can share a group.
  {
    const group = new IsolateGroup();
    const handle = new binding.IsolateGroup();
    const infos = await Promise.all([
      info(kIsolateGroupShared, handle), info(kIsolateGroupShared, handle),
    ]);
    assert.strictEqual(infos[0].isolateGroup, infos[1].isolateGroup);
    assert.notStrictEqual(infos[0].isolateGroup, 0);
    assert.strictEqual(infos[0].usesNodeSnapshot, false);

    const greenThreads = Array.from({ length: 4 }, (_, i) => new GreenThread(
      `onmessage = ({ data }) => postMessage(data + ${i});`,
      { isolateGroup: group }));
    greenThreads.forEach((g) => g.postMessage(10));
    const replies = await Promise.all(
      greenThreads.map(async (g) => (await once(g, 'message'))[0]));
    assert.deepStrictEqual(replies, [10, 11, 12, 13]);
    await Promise.all(greenThreads.map((g) => g.terminate()));
  }

  // Everything else works in a group of its own: messages, cloning, timers,
  // and resource limits, including ArrayBuffers allocated in its sandbox.
  for (const isolateGroup of ['new', new IsolateGroup()]) {
    const greenThread = new GreenThread(`
      onmessage = ({ data }) => setTimeout(() => postMessage({
        bytes: new Uint8Array(data.buffer).reduce((a, b) => a + b, 0),
        map: new Map([[1, data.text]]),
      }), 1);
    `, { isolateGroup });
    greenThread.postMessage({
      buffer: new Uint8Array([1, 2, 3]).buffer,
      text: 'hi',
    });
    const [reply] = await once(greenThread, 'message');
    assert.strictEqual(reply.bytes, 6);
    assert.deepStrictEqual(reply.map, new Map([[1, 'hi']]));
    await greenThread.terminate();

    const limited = new GreenThread(`
      const buffers = [];
      try {
        for (;;) buffers.push(new ArrayBuffer(1024 * 1024));
      } catch {
        postMessage(buffers.length);
      }
      for (;;);
    `, {
      isolateGroup,
      resourceLimits: { maxMemoryMb: 32, maxTaskCpuTimeMs: 50 },
    });
    const [allocated] = await once(limited, 'message');
    assert(allocated > 8 && allocated < 32, `allocated ${allocated} MB`);
    const [error] = await once(limited, 'error');
    assert.strictEqual(error.code, 'ERR_GREEN_THREAD_CPU_LIMIT');
  }
})().then(common.mustCall());
