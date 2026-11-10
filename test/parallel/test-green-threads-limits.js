// Flags: --experimental-green-threads --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { GreenThread } = require('node:green_threads');

// Resolves with the error the green thread stopped with, if any, and its exit
// code.
function run(source, resourceLimits, { evaluate, message, onMessage } = {}) {
  const greenThread = new GreenThread(source, { resourceLimits });
  if (message !== undefined) greenThread.postMessage(message);
  if (onMessage) greenThread.on('message', onMessage);
  let error;
  greenThread.on('error', (e) => { error = e; });
  const evaluated = evaluate === undefined ?
    undefined : greenThread.evaluate(evaluate);
  return new Promise((resolve) => {
    greenThread.on('exit', common.mustCall((code) => {
      resolve({ error, code, evaluated });
    }));
  });
}

async function expectCpuLimit(source, resourceLimits, options) {
  const { error, code, evaluated } = await run(source, resourceLimits, options);
  assert.strictEqual(error?.code, 'ERR_GREEN_THREAD_CPU_LIMIT');
  assert.strictEqual(code, 1);
  return evaluated;
}

(async () => {
  // Code that never yields is preempted when it reaches the per-task limit,
  // wherever it runs.
  const limits = { maxTaskCpuTimeMs: 20 };
  await expectCpuLimit('for (;;);', limits);
  await expectCpuLimit('setTimeout(() => { for (;;); }, 1);', limits);
  await expectCpuLimit('function f() { Promise.resolve().then(f); } f();',
                       limits);
  await expectCpuLimit('onmessage = () => { for (;;); };', limits,
                       { message: 0 });
  await assert.rejects(
    expectCpuLimit('', limits, { evaluate: 'for (;;);' }),
    { code: 'ERR_GREEN_THREAD_EXITED' });
  // Including inside long-running built-ins.
  await expectCpuLimit("/(a+)+b/.test('a'.repeat(40));", limits);

  // The per-task limit applies to each task on its own.
  {
    const ticks = [];
    const { error, code } = await run(`
      let ticks = 0;
      const interval = setInterval(() => {
        const end = Date.now() + 5;
        while (Date.now() < end);
        postMessage(++ticks);
        if (ticks === 10) close();
      }, 1);
    `, { maxTaskCpuTimeMs: 1000 }, { onMessage: (tick) => ticks.push(tick) });
    assert.strictEqual(error, undefined);
    assert.strictEqual(code, 0);
    assert.strictEqual(ticks.length, 10);
  }

  // The overall limit applies to all tasks together.
  {
    let ticks = 0;
    await expectCpuLimit(`
      setInterval(() => {
        const end = Date.now() + 10;
        while (Date.now() < end);
        postMessage(0);
      }, 1);
    `, { maxCpuTimeMs: 50 }, { onMessage: () => ticks++ });
    // Spinning for 10ms of wall time uses at most 10ms of CPU time.
    assert(ticks >= 4, `only ${ticks} tasks ran`);
  }

  // A green thread that reaches its CPU limit doesn't affect others.
  {
    const hog = run('for (;;);', { maxCpuTimeMs: 100 });
    const results = await Promise.all(Array.from({ length: 20 }, (_, i) => {
      return run(`postMessage(${i}); close();`, { maxCpuTimeMs: 1000 });
    }));
    for (const { error, code } of results) {
      assert.strictEqual(error, undefined);
      assert.strictEqual(code, 0);
    }
    const { error } = await hog;
    assert.strictEqual(error.code, 'ERR_GREEN_THREAD_CPU_LIMIT');
  }

  // maxMemoryMb covers ArrayBuffers: allocations that would exceed it throw
  // a RangeError, which can be caught.
  {
    const messages = [];
    const { error, code } = await run(`
      try {
        new ArrayBuffer(64 * 1024 * 1024);
      } catch (error) {
        postMessage(error.name);
      }
      const buffers = [];
      for (let i = 0; i < 16; i++) buffers.push(new ArrayBuffer(1024 * 1024));
      postMessage(buffers.length);
      try {
        for (;;) buffers.push(new ArrayBuffer(1024 * 1024));
      } catch (error) {
        postMessage(buffers.length);
      }
      close();
    `, { maxMemoryMb: 32 }, { onMessage: (m) => messages.push(m) });
    assert.strictEqual(error, undefined);
    assert.strictEqual(code, 0);
    assert.strictEqual(messages[0], 'RangeError');
    assert.strictEqual(messages[1], 16);
    assert(messages[2] < 32, `allocated ${messages[2]} MB`);
  }

  // ...and the JS heap.
  {
    const { error, code } = await run(
      'const items = []; for (;;) items.push({ length: items.length });',
      { maxMemoryMb: 32 });
    assert.strictEqual(error.code, 'ERR_GREEN_THREAD_OUT_OF_MEMORY');
    assert.strictEqual(code, 1);
  }

  // ...together: memory that is kept alive in either counts towards it.
  {
    const { error, code } = await run(`
      const buffers = [];
      for (let i = 0; i < 24; i++) buffers.push(new ArrayBuffer(1024 * 1024));
      const items = [];
      // Not \`new Array(n)\`: V8 may optimize such a loop so that it no
      // longer handles interrupts.
      for (;;) items.push([items.length, items.length, items.length]);
    `, { maxMemoryMb: 32 });
    assert.strictEqual(error.code, 'ERR_GREEN_THREAD_OUT_OF_MEMORY');
    assert.strictEqual(code, 1);
  }

  // Memory and CPU limits can be combined.
  await expectCpuLimit('for (;;);', { maxMemoryMb: 32, maxTaskCpuTimeMs: 20 });

  for (const name of ['maxMemoryMb', 'maxCpuTimeMs', 'maxTaskCpuTimeMs']) {
    assert.throws(() => new GreenThread('', { resourceLimits: { [name]: -1 } }),
                  { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => new GreenThread('', { resourceLimits: { [name]: '1' } }),
                  { code: 'ERR_INVALID_ARG_TYPE' });
  }
})().then(common.mustCall());
