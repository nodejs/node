// Flags: --experimental-green-threads --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const util = require('util');
const { GreenThread } = require('node:green_threads');

async function expectError(source, expected, options, message) {
  const greenThread = new GreenThread(source, options);
  greenThread.on('message', common.mustNotCall());
  if (message !== undefined) greenThread.postMessage(message);
  const [error, code] = await Promise.all([
    new Promise((resolve) => greenThread.on('error', common.mustCall(resolve))),
    new Promise((resolve) => greenThread.on('exit', common.mustCall(resolve))),
  ]);
  assert.throws(() => { throw error; }, expected);
  assert.strictEqual(code, 1);
  assert.strictEqual(greenThread.exitCode, 1);
}

(async () => {
  await expectError('throw new RangeError("top level")',
                    { name: 'RangeError', message: 'top level' });
  await expectError('Promise.reject(new Error("unhandled"))',
                    { name: 'Error', message: 'unhandled' });
  await expectError('queueMicrotask(() => { throw new Error("microtask"); })',
                    { name: 'Error', message: 'microtask' });
  await expectError('setTimeout(() => { throw new Error("timer"); }, 1)',
                    { name: 'Error', message: 'timer' });
  await expectError('onmessage = () => { throw new TypeError("handler"); };',
                    { name: 'TypeError', message: 'handler' },
                    undefined,
                    'trigger');
  // An error stops the green thread before later code runs.
  await expectError(`
    queueMicrotask(() => { throw new Error('first'); });
    queueMicrotask(() => postMessage('unreachable'));
  `, { message: 'first' });
  // Values thrown that can't be cloned are reported as strings.
  await expectError('throw { fn() {} }',
                    (error) => error === '[object Object]');
  await expectError('const items = []; for (;;) items.push(new Array(1e4));',
                    { code: 'ERR_GREEN_THREAD_OUT_OF_MEMORY' },
                    { resourceLimits: { maxOldGenerationSizeMb: 16 } });
  // No JS runs after reaching the heap limit, not even queued microtasks.
  await expectError(`
    queueMicrotask(() => postMessage('unreachable'));
    const items = [];
    for (;;) items.push(new Array(1e4));
  `, { code: 'ERR_GREEN_THREAD_OUT_OF_MEMORY' },
                    { resourceLimits: { maxOldGenerationSizeMb: 16 } });

  // A rejection that is handled in the same task is not an error.
  {
    const greenThread = new GreenThread(`
      const promise = Promise.reject(new Error('handled'));
      queueMicrotask(() => promise.catch(() => postMessage('caught')));
    `);
    greenThread.on('error', common.mustNotCall());
    assert.deepStrictEqual(await once(greenThread, 'message'), ['caught']);
    await greenThread.terminate();
  }

  // Errors keep their class and data properties, like those of workers, and
  // are real errors on the host.
  {
    const greenThread = new GreenThread(`
      class CustomError extends RangeError {
        constructor(message, options) {
          super(message, options);
          this.name = 'CustomError';
          this.code = 'ERR_CUSTOM';
          this.details = { count: 1 };
        }
      }
      globalThis.CustomError = CustomError;
      globalThis.withCause = () => new CustomError('outer', {
        cause: Object.assign(new TypeError('inner'), { code: 'ERR_INNER' }),
      });
    `, {
      env: {
        fail() {
          throw Object.assign(new TypeError('from the host'), {
            code: 'ERR_HOST',
          });
        },
      },
    });
    const checkCustom = common.mustCall((error) => {
      assert.ok(util.types.isNativeError(error));
      assert.ok(error instanceof RangeError);
      assert.strictEqual(error.name, 'CustomError');
      assert.strictEqual(error.message, 'outer');
      assert.strictEqual(error.code, 'ERR_CUSTOM');
      assert.deepStrictEqual(error.details, { count: 1 });
      assert.ok(error.cause instanceof TypeError);
      assert.strictEqual(error.cause.code, 'ERR_INNER');
      assert.match(error.stack, /^CustomError: outer\n {4}at /);
      return true;
    }, 3);
    await assert.rejects(greenThread.evaluate('throw withCause()'),
                         checkCustom);
    await assert.rejects(greenThread.evaluate('Promise.reject(withCause())'),
                         checkCustom);

    // Properties that can't be cloned are dropped, not the error.
    await assert.rejects(greenThread.evaluate(`
      throw Object.assign(new Error('partial'), {
        code: 'ERR_PARTIAL',
        handler: { fn() {} },
      });
    `), (error) => {
      assert.strictEqual(error.code, 'ERR_PARTIAL');
      assert.strictEqual(error.handler, undefined);
      return true;
    });

    // AggregateErrors keep their errors' properties.
    await assert.rejects(greenThread.evaluate(`
      throw new AggregateError(
        [Object.assign(new Error('one'), { code: 'ERR_ONE' }), 2], 'all');
    `), (error) => {
      assert.ok(error instanceof AggregateError);
      assert.strictEqual(error.errors[0].code, 'ERR_ONE');
      assert.strictEqual(error.errors[1], 2);
      return true;
    });

    // DOMExceptions stay DOMExceptions.
    await assert.rejects(
      greenThread.evaluate('throw new DOMException("stop", "AbortError")'),
      (error) => error instanceof DOMException && error.name === 'AbortError');

    // In RPC, in both directions.
    assert.deepStrictEqual(await greenThread.evaluate(`
      env.fail().catch((error) => [error.constructor.name, error.code]);
    `), ['TypeError', 'ERR_HOST']);

    // Uncaught.
    greenThread.evaluate('setTimeout(() => { throw withCause(); }, 1)');
    const [error] = await once(greenThread, 'error');
    checkCustom(error);
  }

  // So do those of calls to the exports of modules.
  {
    const greenThread = new GreenThread(`
      export function fail() {
        throw Object.assign(new SyntaxError('export'), { code: 'ERR_EXPORT' });
      }
    `, { type: 'module' });
    const { fail } = await greenThread.exports;
    await assert.rejects(fail(), {
      name: 'SyntaxError',
      code: 'ERR_EXPORT',
    });
    await greenThread.terminate();
  }
})().then(common.mustCall());

// The message handler is only invoked when set.
{
  const greenThread = new GreenThread('');
  greenThread.postMessage('dropped');
  (async () => {
    await greenThread.evaluate('onmessage = ({ data }) => postMessage(data); 0');
    greenThread.postMessage('delivered');
  })().then(common.mustCall());
  greenThread.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 'delivered');
    greenThread.terminate().then(common.mustCall());
  }));
}
