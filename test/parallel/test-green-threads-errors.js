// Flags: --experimental-green-threads --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
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
  await expectError('throw { fn() {} }', (error) => error === '#<Object>');
  await expectError('const items = []; for (;;) items.push(new Array(1e4));',
                    { code: 'ERR_GREEN_THREAD_OUT_OF_MEMORY' },
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
