// Flags: --experimental-green-threads --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { GreenThread } = require('node:green_threads');

(async () => {
  // Messages in both directions.
  {
    const greenThread = new GreenThread(`
      onmessage = ({ data }) => postMessage({ doubled: data.value * 2 });
    `);
    greenThread.postMessage({ value: 21 });
    const [message] = await once(greenThread, 'message');
    assert.deepStrictEqual(message, { doubled: 42 });
    assert.strictEqual(greenThread.exitCode, undefined);
    assert.strictEqual(await greenThread.terminate(), 1);
    assert.strictEqual(greenThread.exitCode, 1);
    assert.strictEqual(await greenThread.terminate(), 1);
  }

  // Only ECMAScript built-ins and the green thread globals are available.
  {
    const greenThread = new GreenThread('');
    assert.deepStrictEqual(await greenThread.evaluate(`[
      typeof require, typeof process, typeof Buffer, typeof module,
      typeof postMessage, typeof queueMicrotask, typeof setTimeout,
      typeof clearTimeout, typeof setInterval, typeof clearInterval,
      typeof close, typeof console.log,
    ]`), [
      'undefined', 'undefined', 'undefined', 'undefined',
      'function', 'function', 'function',
      'function', 'function', 'function',
      'function', 'function',
    ]);
    await greenThread.terminate();
  }

  // evaluate() keeps state, awaits promises and reports errors without
  // stopping the green thread.
  {
    const greenThread = new GreenThread('globalThis.counter = 0;');
    assert.strictEqual(await greenThread.evaluate('++counter'), 1);
    assert.strictEqual(await greenThread.evaluate('++counter'), 2);
    assert.deepStrictEqual(
      await greenThread.evaluate(
        'new Promise((resolve) => setTimeout(() => resolve([counter]), 1))'),
      [2]);
    await assert.rejects(greenThread.evaluate('throw new TypeError("boom")'), {
      name: 'TypeError',
      message: 'boom',
    });
    await assert.rejects(greenThread.evaluate('Promise.reject(new Error("no"))'),
                         { message: 'no' });
    await assert.rejects(greenThread.evaluate('() => {}'),
                         { name: 'Error', message: /could not be cloned/ });
    await assert.rejects(greenThread.evaluate('syntax error'),
                         { name: 'SyntaxError' });
    assert.strictEqual(await greenThread.evaluate('counter'), 2);
    await greenThread.terminate();
  }

  // Timers and microtasks run in order.
  {
    const greenThread = new GreenThread(`
      const order = [];
      let pending = 2;
      const done = () => { if (--pending === 0) postMessage(order); };
      setTimeout(() => { order.push('timeout'); done(); }, 2);
      const cancelled = setTimeout(() => order.push('cancelled'), 1);
      clearTimeout(cancelled);
      let ticks = 0;
      const interval = setInterval(() => {
        order.push('interval');
        if (++ticks === 2) {
          clearInterval(interval);
          // Give a cleared interval the chance to fire again.
          setTimeout(done, 5);
        }
      }, 1);
      queueMicrotask(() => order.push('microtask'));
      Promise.resolve().then(() => order.push('promise'));
      order.push('sync');
    `);
    const [order] = await once(greenThread, 'message');
    assert.deepStrictEqual(order.slice(0, 3), ['sync', 'microtask', 'promise']);
    assert.strictEqual(order.filter((x) => x === 'interval').length, 2);
    assert(order.includes('timeout'));
    assert(!order.includes('cancelled'));
    await greenThread.terminate();
  }

  // close() exits with code 0, after the current task.
  {
    const greenThread = new GreenThread('postMessage("before"); close(); ' +
                                        'postMessage("after");');
    const messages = [];
    greenThread.on('message', (message) => messages.push(message));
    const [code] = await once(greenThread, 'exit');
    assert.strictEqual(code, 0);
    assert.deepStrictEqual(messages, ['before', 'after']);
    await assert.rejects(greenThread.evaluate('1'),
                         { code: 'ERR_GREEN_THREAD_EXITED' });
    greenThread.postMessage('ignored');
  }

  // Pending evaluations are rejected when the green thread exits.
  {
    const greenThread = new GreenThread('');
    const pending = greenThread.evaluate('new Promise(() => {})');
    greenThread.terminate();
    await assert.rejects(pending, { code: 'ERR_GREEN_THREAD_EXITED' });
  }

  // terminate() interrupts running code.
  {
    const greenThread = new GreenThread('for (;;);');
    assert.strictEqual(await greenThread.terminate(), 1);
  }

  // Values that can't be cloned throw.
  {
    const greenThread = new GreenThread('');
    assert.throws(() => greenThread.postMessage(() => {}), {
      name: 'DataCloneError',
    });
    await assert.rejects(greenThread.evaluate('postMessage(Symbol())'), {
      name: 'Error',
    });
    await greenThread.terminate();
  }

  // The console methods are forwarded to the host's console.
  {
    const originalLog = console.log;
    const originalError = console.error;
    const greenThread = new GreenThread(`
      console.log('hello', 1, { a: true });
      console.error(() => 'not cloneable');
    `);
    const logged = new Promise((resolve) => {
      console.log = common.mustCall((...args) => resolve(args));
    });
    const errored = new Promise((resolve) => {
      console.error = common.mustCall((...args) => resolve(args));
    });
    try {
      assert.deepStrictEqual(await logged, ['hello', 1, { a: true }]);
      assert.deepStrictEqual(await errored, ["() => 'not cloneable'"]);
    } finally {
      console.log = originalLog;
      console.error = originalError;
    }
    await greenThread.terminate();
  }

  // Many green threads can run at once.
  {
    const greenThreads = Array.from({ length: 200 }, () => new GreenThread(
      'onmessage = ({ data }) => postMessage(data + 1);'));
    const results = await Promise.all(greenThreads.map((greenThread, i) => {
      const message = once(greenThread, 'message');
      greenThread.postMessage(i);
      return message;
    }));
    assert.deepStrictEqual(results.map(([value]) => value),
                           greenThreads.map((_, i) => i + 1));
    await Promise.all(greenThreads.map((greenThread) => greenThread.terminate()));
  }

  // Symbol.asyncDispose terminates the green thread.
  {
    const greenThread = new GreenThread('setInterval(() => {}, 1000);');
    await greenThread[Symbol.asyncDispose]();
    assert.strictEqual(greenThread.exitCode, 1);
  }
})().then(common.mustCall());
