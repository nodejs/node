// Flags: --expose-gc --expose-internals --experimental-green-threads --no-warnings
'use strict';

// RPC between green threads and their host: the env, RpcTargets, functions,
// errors, AbortSignals, and releasing what the other side no longer uses.

const common = require('../common');
const assert = require('assert');
const { GreenThread, RpcTarget } = require('node:green_threads');
const { kTestingFlags } = require('internal/green_threads/testing');
const { internalBinding } = require('internal/test/binding');
const { kFlagTestingGlobals } = internalBinding('green_thread');

// Runs `source` in an async function in a green thread with `env`, and
// resolves with what it returns.
async function run(source, env, options = {}) {
  const greenThread = new GreenThread(`
    (async () => { ${source} })().then(
      (value) => postMessage({ value }),
      (error) => postMessage({ error }));
  `, { env, ...options });
  try {
    const [result] = await new Promise((resolve, reject) => {
      greenThread.once('message', (message) => resolve([message]));
      greenThread.once('error', reject);
    });
    if ('error' in result) throw result.error;
    return result.value;
  } finally {
    await greenThread.terminate();
  }
}

async function gc() {
  for (let i = 0; i < 4; i++) {
    globalThis.gc();
    await new Promise((resolve) => setTimeout(resolve, 5));
  }
}

class Counter extends RpcTarget {
  #count = 0;
  field = 'not passed';
  increment(by = 1) { return this.#count += by; }
  get count() { return this.#count; }
}

class SubCounter extends Counter {
  decrement() { return this.increment(-1); }
}

(async () => {
  // The env: values are cloned, functions anywhere in plain objects and
  // arrays become stubs.
  {
    const store = new Map();
    const result = await run(`
      await env.db.put('key', { value: 1 });
      return [
        env.name, env.map instanceof Map, typeof env.db.get,
        typeof env.list[1], await env.db.get('key'), await env.list[1](2),
        Object.keys(globalThis).includes('env'),
      ];
    `, {
      name: 'name',
      map: new Map(),
      db: {
        get: (key) => store.get(key),
        put: (key, value) => { store.set(key, value); },
      },
      list: [1, (x) => x * 2],
    });
    assert.deepStrictEqual(result, [
      'name', true, 'function', 'function', { value: 1 }, 4, false,
    ]);
  }

  // No env.
  assert.strictEqual(await run('return typeof env;'), 'undefined');
  assert.throws(() => new GreenThread('', { env: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  // Functions in non-plain objects can't be cloned.
  assert.throws(() => new GreenThread('', { env: { map: new Map([[1, () => {}]]) } }), {
    name: 'DataCloneError',
  });

  // RpcTargets: only their methods are passed, including inherited ones.
  {
    const counter = new SubCounter();
    const result = await run(`
      const counter = await env.getCounter();
      return [
        Object.keys(counter).sort(), await counter.increment(5),
        await counter.decrement(), counter.count, counter.field,
        typeof counter.constructor, counter instanceof RpcTarget,
      ];
    `, { getCounter: () => counter });
    assert.deepStrictEqual(result, [
      ['decrement', 'increment'], 5, 4, undefined, undefined, 'undefined',
      false,
    ]);
    assert.strictEqual(counter.count, 4);
  }

  // Green threads export RpcTargets and functions too. A stub passed back
  // becomes the original object.
  {
    let target;
    let callback;
    const result = await run(`
      class Greeter extends RpcTarget {
        greet(name) { return 'hello ' + name; }
      }
      const greeter = new Greeter();
      const add = (a, b) => a + b;
      await env.register(greeter, add);
      return [await env.callBack(), await env.isSame(greeter, add)];
    `, {
      register(greeter, add) {
        target = greeter;
        callback = add;
      },
      async callBack() {
        return `${await target.greet('host')} ${await callback(1, 2)}`;
      },
      isSame: (greeter, add) => greeter === target && add === callback,
    });
    assert.deepStrictEqual(result, ['hello host 3', true]);
    // Pending and later calls are rejected once the green thread is gone.
    await assert.rejects(target.greet('host'), {
      code: 'ERR_GREEN_THREAD_EXITED',
    });
  }

  // A stub returned to the side that exported it is the original object.
  {
    const counter = new Counter();
    assert.strictEqual(await run(`
      return env.isOriginal(await env.getCounter());
    `, {
      getCounter: () => counter,
      isOriginal: (value) => value === counter,
    }), true);
  }

  // Errors.
  {
    const result = await run(`
      const results = [];
      for (const method of ['type', 'dom', 'function', 'value']) {
        try {
          await env.fail(method);
        } catch (error) {
          results.push([error.constructor.name, error.name, error.message]);
        }
      }
      try {
        await env.fail(() => {}, { notCloneable: Symbol() });
      } catch (error) {
        results.push([error.name]);
      }
      return results;
    `, {
      fail(kind) {
        switch (kind) {
          case 'type': throw new TypeError('type');
          case 'dom': throw new DOMException('dom', 'AbortError');
          // eslint-disable-next-line no-throw-literal
          case 'function': throw function notCloneable() {};
          // eslint-disable-next-line no-throw-literal
          case 'value': throw 42;
        }
      },
    });
    assert.deepStrictEqual(result, [
      ['TypeError', 'TypeError', 'type'],
      ['DOMException', 'AbortError', 'dom'],
      ['String', undefined, undefined],
      ['Number', undefined, undefined],
      ['DataCloneError'],
    ]);
  }

  // AbortSignals, in both directions, aborted before and after the call.
  {
    const result = await run(`
      const controller = new AbortController();
      const later = env.whenAborted(controller.signal);
      controller.abort('reason');
      return [
        await later,
        await env.whenAborted(AbortSignal.abort('already')),
        await env.whenAborted(
          AbortSignal.abort(new DOMException('dom', 'TimeoutError'))),
        await env.abortGreen(async (signal) => {
          await new Promise((resolve) => { signal.onabort = resolve; });
          return [signal instanceof AbortSignal, signal.reason];
        }),
      ];
    `, {
      whenAborted(signal) {
        if (!(signal instanceof AbortSignal)) return 'not an AbortSignal';
        if (signal.aborted) {
          return `already aborted: ${signal.reason?.name ?? signal.reason}`;
        }
        return new Promise((resolve) => {
          signal.addEventListener('abort', () => {
            resolve(`aborted: ${signal.reason}`);
          });
        });
      },
      abortGreen(callback) {
        const controller = new AbortController();
        const result = callback(controller.signal);
        setTimeout(() => controller.abort('host'), 1);
        return result;
      },
    });
    assert.deepStrictEqual(result, [
      'aborted: reason', 'already aborted: already',
      'already aborted: TimeoutError', [true, 'host'],
    ]);
  }

  // Stubs that are disposed, or no longer used, release what they refer to.
  {
    let weak;
    let otherWeak;
    const greenThread = new GreenThread(`
      onmessage = async () => {
        let counter = await env.getCounter();
        counter[Symbol.dispose]();
        const result = await counter.increment().catch((error) => error.code);
        let other = await env.getOther();
        await other.increment();
        other = counter = null;
        for (let i = 0; i < 4; i++) {
          gc();
          await new Promise((resolve) => setTimeout(resolve, 5));
        }
        postMessage(result);
      };
    `, {
      env: {
        getCounter() {
          const counter = new Counter();
          weak = new WeakRef(counter);
          return counter;
        },
        getOther() {
          const other = new Counter();
          otherWeak = new WeakRef(other);
          return other;
        },
      },
    });
    greenThread.postMessage(0);
    const [result] = await new Promise((resolve) => {
      greenThread.once('message', (message) => resolve([message]));
    });
    assert.strictEqual(result, 'ERR_GREEN_THREAD_RPC_DISPOSED');
    await gc();
    assert.strictEqual(weak.deref(), undefined);
    assert.strictEqual(otherWeak.deref(), undefined);
    await greenThread.terminate();
  }

  // The result of a call continues in the invocation context of the caller,
  // which is kept as long as calls are pending.
  {
    const greenThread = new GreenThread(`
      onmessage = async () => {
        const before = __greenThreadInfo();
        await env.wait();
        const after = __greenThreadInfo();
        await new Promise((resolve) => setTimeout(resolve, 1));
        const timer = __greenThreadInfo();
        postMessage([before.context === after.context,
                     before.context === timer.context,
                     after.thread === timer.thread]);
      };
    `, {
      env: { wait: () => new Promise((resolve) => setTimeout(resolve, 10)) },
      [kTestingFlags]: kFlagTestingGlobals,
    });
    greenThread.postMessage(0);
    const [result] = await new Promise((resolve) => {
      greenThread.once('message', (message) => resolve([message]));
    });
    assert.deepStrictEqual(result, [true, true, true]);
    // The default context, and that of the evaluation.
    assert.strictEqual(
      await greenThread.evaluate('__greenThreadInfo().contexts'), 2);
    await greenThread.terminate();
  }

  // Calls that are pending when a green thread exits are rejected.
  {
    let hang;
    const greenThread = new GreenThread(`
      env.register(() => new Promise(() => {}));
    `, { env: { register: (fn) => { hang = fn; } } });
    await new Promise((resolve) => {
      const check = () => (hang ? resolve() : setTimeout(check, 1));
      check();
    });
    const pending = hang();
    await greenThread.terminate();
    await assert.rejects(pending, { code: 'ERR_GREEN_THREAD_EXITED' });
  }
})().then(common.mustCall());
