// Flags: --experimental-green-threads --no-warnings
'use strict';

// The web APIs that green threads have: EventTarget, Event, AbortController,
// AbortSignal and DOMException.

const common = require('../common');
const assert = require('assert');
const { GreenThread } = require('node:green_threads');

async function run(source) {
  const greenThread = new GreenThread('');
  try {
    return await greenThread.evaluate(`(async () => { ${source} })()`);
  } finally {
    await greenThread.terminate();
  }
}

(async () => {
  // The globals, which are not enumerable.
  const result1 = await run(`
    return [
      typeof EventTarget, typeof Event, typeof AbortController,
      typeof AbortSignal, typeof DOMException, Object.keys(globalThis),
    ];
  `);
  assert.deepStrictEqual(result1, [
    'function', 'function', 'function', 'function', 'function', ['onmessage'],
  ]);

  // EventTarget and Event.
  const result2 = await run(`
    const calls = [];
    const target = new EventTarget();
    const listener = (event) => calls.push('function:' + event.eventPhase);
    target.addEventListener('x', listener);
    target.addEventListener('x', listener);  // Added once only.
    target.addEventListener('x', { handleEvent() { calls.push('object'); } });
    target.addEventListener('x', () => calls.push('once'), { once: true });
    const event = new Event('x', { cancelable: true });
    target.addEventListener('x', (event) => event.preventDefault());
    calls.push(target.dispatchEvent(event), event.defaultPrevented,
               event.target === target, event.eventPhase, event.isTrusted);
    target.removeEventListener('x', listener);
    target.dispatchEvent(new Event('x'));
    try { new Event(); } catch (error) { calls.push(error.code); }
    try {
      target.dispatchEvent({});
    } catch (error) {
      calls.push(error.code);
    }
    return calls;
  `);
  assert.deepStrictEqual(result2, [
    'function:2', 'object', 'once', false, true, true, 0, false, 'object',
    'ERR_MISSING_ARGS', 'ERR_INVALID_ARG_TYPE',
  ]);

  // Listener errors don't stop the dispatch, and are uncaught errors.
  {
    const greenThread = new GreenThread(`
      const target = new EventTarget();
      target.addEventListener('x', () => { throw new Error('boom'); });
      target.addEventListener('x', () => postMessage('second listener'));
      target.dispatchEvent(new Event('x'));
    `);
    greenThread.on('message', common.mustCall((message) => {
      assert.strictEqual(message, 'second listener');
    }));
    greenThread.on('error', common.mustCall((error) => {
      assert.strictEqual(error.message, 'boom');
    }));
    await new Promise((resolve) => greenThread.once('exit', resolve));
  }

  // AbortController and AbortSignal.
  const result3 = await run(`
    const results = [];
    const controller = new AbortController();
    const { signal } = controller;
    signal.onabort = (event) => results.push('onabort:' + event.isTrusted);
    signal.addEventListener('abort', () => results.push('listener'));
    results.push(signal.aborted);
    controller.abort();
    controller.abort();
    results.push(signal.aborted, signal.reason instanceof DOMException,
                 signal.reason.name, String(signal));
    try { signal.throwIfAborted(); } catch (error) {
      results.push(error === signal.reason);
    }

    results.push(AbortSignal.abort(1).reason);
    const timeout = AbortSignal.timeout(1);
    await new Promise((resolve) => timeout.onabort = resolve);
    results.push(timeout.reason.name);

    const source = new AbortController();
    const any = AbortSignal.any([new AbortController().signal, source.signal]);
    source.abort(2);
    results.push(any.aborted, any.reason);
    results.push(AbortSignal.any([AbortSignal.abort(3)]).reason);

    // A listener can be removed with a signal.
    const target = new EventTarget();
    const remover = new AbortController();
    target.addEventListener('x', () => results.push('not removed'),
                            { signal: remover.signal });
    remover.abort();
    target.dispatchEvent(new Event('x'));

    try { new AbortSignal(); } catch (error) { results.push(error.code); }
    return results;
  `);
  assert.deepStrictEqual(result3, [
    false, 'onabort:true', 'listener', true, true, 'AbortError',
    '[object AbortSignal]', true, 1, 'TimeoutError', true, 2, 3,
    'ERR_ILLEGAL_CONSTRUCTOR',
  ]);

  // DOMException is Node.js' own.
  const result4 = await run(`
    const error = new DOMException('message', 'AbortError');
    return [error.name, error.message, error.code, error instanceof Error,
            DOMException.ABORT_ERR];
  `);
  assert.deepStrictEqual(result4, ['AbortError', 'message', 20, true, 20]);

  // Changing built-ins doesn't affect the runtime.
  const result5 = await run(`
    Array.prototype.push = () => { throw new Error('push'); };
    Map.prototype.get = () => { throw new Error('get'); };
    Map.prototype.set = () => { throw new Error('set'); };
    Function.prototype.call = () => { throw new Error('call'); };
    globalThis.setTimeout = () => { throw new Error('setTimeout'); };
    const controller = new AbortController();
    let aborted = false;
    controller.signal.addEventListener('abort', () => { aborted = true; });
    controller.abort();
    const timeout = AbortSignal.timeout(1);
    await new Promise((resolve) => { timeout.onabort = resolve; });
    return [aborted, timeout.aborted];
  `);
  assert.deepStrictEqual(result5, [true, true]);
})().then(common.mustCall());
