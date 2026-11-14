// Flags: --experimental-green-threads --no-warnings
'use strict';

// greenThread.close() calls the green thread's onclose, and makes it exit
// once nothing it waits for is pending, or terminates it after a timeout.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { GreenThread } = require('node:green_threads');

{
  const greenThread = new GreenThread('');
  for (const timeoutMs of [-1, 1.5, 2 ** 31, NaN]) {
    assert.throws(() => greenThread.close({ timeoutMs }),
                  { code: 'ERR_OUT_OF_RANGE' });
  }
  for (const options of [null, 1]) {
    assert.throws(() => greenThread.close(options),
                  { code: 'ERR_INVALID_ARG_TYPE' });
  }
  assert.throws(() => greenThread.close({ timeoutMs: '1' }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  greenThread.terminate();
}

(async () => {
  // Without anything pending, it exits right away, with exit code 0.
  {
    const greenThread = new GreenThread('');
    const closed = greenThread.close();
    assert.strictEqual(greenThread.close(), closed);
    assert.strictEqual(await closed, 0);
    assert.strictEqual(greenThread.exitCode, 0);
    assert.strictEqual(await greenThread.close(), 0);
  }

  // The onclose function is called, and the timers it sets are waited for.
  {
    const greenThread = new GreenThread(`
      onclose = () => {
        setTimeout(() => postMessage('flushed'), 20);
      };
    `);
    const messages = [];
    greenThread.on('message', (message) => messages.push(message));
    assert.strictEqual(await greenThread.close(), 0);
    assert.deepStrictEqual(messages, ['flushed']);
  }

  // Messages posted before close() are delivered, later ones are dropped,
  // and evaluate() rejects.
  {
    const greenThread = new GreenThread(
      'onmessage = ({ data }) => postMessage(data);');
    const messages = [];
    greenThread.on('message', (message) => messages.push(message));
    greenThread.postMessage(1);
    const closed = greenThread.close();
    greenThread.postMessage(2);
    await assert.rejects(greenThread.evaluate('1'),
                         { code: 'ERR_GREEN_THREAD_CLOSING' });
    assert.strictEqual(await closed, 0);
    assert.deepStrictEqual(messages, [1]);
  }

  // Evaluations, RPC calls from the host, and the green thread's own RPC
  // calls are waited for.
  {
    const slow = (value) => new Promise((resolve) => {
      setTimeout(() => resolve(value), 30);
    });
    const greenThread = new GreenThread(`
      export async function work() {
        return env.slow(42);
      }
    `, { type: 'module', env: { slow } });
    const { work } = await greenThread.exports;
    const evaluated = greenThread.evaluate('env.slow(7)');
    const worked = work();
    const closed = greenThread.close();
    assert.strictEqual(await evaluated, 7);
    assert.strictEqual(await worked, 42);
    assert.strictEqual(await closed, 0);
  }

  // The green thread can call close() itself.
  {
    const greenThread = new GreenThread(`
      onclose = () => close();
      setInterval(() => {}, 10);
    `);
    assert.strictEqual(await greenThread.close({ timeoutMs: Infinity }), 0);
  }

  // Open ports don't keep it running.
  {
    const { port1, port2 } = new MessageChannel();
    const greenThread = new GreenThread(`
      onmessage = ({ ports }) => {
        ports[0].onmessage = () => {};
        postMessage('ready');
      };
    `);
    greenThread.postMessage(null, [port2]);
    await once(greenThread, 'message');
    const closed = once(port1, 'close');
    assert.strictEqual(await greenThread.close(), 0);
    await closed;
  }

  // Something that stays pending, like an interval, makes it time out, and
  // it is terminated.
  {
    const greenThread = new GreenThread('setInterval(() => {}, 10);');
    assert.strictEqual(await greenThread.close({ timeoutMs: 50 }), 1);
  }
  {
    const greenThread = new GreenThread('setInterval(() => {}, 10);');
    const closed = greenThread.close({ timeoutMs: Infinity });
    await new Promise((resolve) => setTimeout(resolve, 50));
    assert.strictEqual(greenThread.exitCode, undefined);
    await greenThread.terminate();
    assert.strictEqual(await closed, 1);
  }
})().then(common.mustCall());
