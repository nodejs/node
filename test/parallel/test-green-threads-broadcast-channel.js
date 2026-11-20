// Flags: --experimental-green-threads --no-warnings
'use strict';

// Green threads can use Node.js' BroadcastChannels, process-wide, for the
// names in their broadcastScopes.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { GreenThread } = require('node:green_threads');
const { Worker } = require('worker_threads');

for (const broadcastScopes of [null, 'foo', {}]) {
  assert.throws(() => new GreenThread('', { broadcastScopes }),
                { code: 'ERR_INVALID_ARG_TYPE' });
}
assert.throws(() => new GreenThread('', { broadcastScopes: ['foo', 1] }),
              { code: 'ERR_INVALID_ARG_TYPE',
                message: /options\.broadcastScopes\[1\]/ });

// A green thread that sends the host what its channel receives, and posts
// what the host sends it to its channel.
async function relay(name, options) {
  const greenThread = new GreenThread(`
    const channel = new BroadcastChannel(${JSON.stringify(name)});
    channel.onmessage = ({ data }) => postMessage({ type: 'message', data });
    channel.onmessageerror = ({ data }) => {
      postMessage({ type: 'messageerror', name: data.name });
    };
    onmessage = ({ data }) => {
      if (data === 'close') {
        channel.close();
        postMessage({ type: 'closed' });
      } else {
        channel.postMessage(data);
      }
    };
    postMessage('ready');
  `, options);
  const [ready] = await once(greenThread, 'message');
  assert.strictEqual(ready, 'ready');
  return greenThread;
}

// The next message of a green thread from relay().
async function next(greenThread) {
  const [message] = await once(greenThread, 'message');
  return message;
}

(async () => {
  // Other names, or none, throw.
  for (const options of [{}, { broadcastScopes: ['bar'] }]) {
    const greenThread = new GreenThread('', options);
    assert.deepStrictEqual(await greenThread.evaluate(`
      const errors = [];
      for (const name of ['foo', '']) {
        try { new BroadcastChannel(name); } catch (error) {
          errors.push(error.code);
        }
      }
      try { new BroadcastChannel(); } catch (error) { errors.push(error.code); }
      errors;
    `), [
      'ERR_GREEN_THREAD_BROADCAST_SCOPE',
      'ERR_GREEN_THREAD_BROADCAST_SCOPE',
      'ERR_MISSING_ARGS',
    ]);
    await greenThread.terminate();
  }

  // The API.
  {
    const greenThread = new GreenThread('', { broadcastScopes: ['foo'] });
    assert.deepStrictEqual(await greenThread.evaluate(`
      const channel = new BroadcastChannel('foo');
      const result = [
        channel.name,
        Object.prototype.toString.call(channel),
        channel instanceof EventTarget,
      ];
      try { channel.postMessage(); } catch (error) { result.push(error.code); }
      try { channel.postMessage(() => {}); } catch (error) {
        result.push(error.name);
      }
      channel.close();
      channel.close();
      try { channel.postMessage(1); } catch (error) {
        result.push(error.name, error instanceof DOMException);
      }
      result;
    `), [
      'foo',
      '[object BroadcastChannel]',
      true,
      'ERR_MISSING_ARGS',
      'DataCloneError',
      'InvalidStateError',
      true,
    ]);
    await greenThread.terminate();
  }

  // Channels with the same name are the same everywhere: in the host, in
  // workers and in green threads. Messages don't reach their sender.
  {
    const name = 'shared';
    const host = new BroadcastChannel(name);
    const hostMessages = [];
    host.onmessage = ({ data }) => hostMessages.push(data);
    const a = await relay(name, { broadcastScopes: ['other', name] });
    const b = await relay(name, { broadcastScopes: [name] });

    // Listen before posting: both receive at the same time.
    let received = Promise.all([next(a), next(b)]);
    host.postMessage('from host');
    assert.deepStrictEqual(await received, [
      { type: 'message', data: 'from host' },
      { type: 'message', data: 'from host' },
    ]);

    a.postMessage('from a');
    assert.deepStrictEqual(await next(b), { type: 'message', data: 'from a' });

    received = Promise.all([next(a), next(b)]);
    const worker = new Worker(`
      const channel = new BroadcastChannel(${JSON.stringify(name)});
      channel.postMessage({ from: 'worker' });
      channel.close();
    `, { eval: true });
    await once(worker, 'exit');
    assert.deepStrictEqual(await received, [
      { type: 'message', data: { from: 'worker' } },
      { type: 'message', data: { from: 'worker' } },
    ]);
    while (hostMessages.length < 2) {
      await new Promise((resolve) => setImmediate(resolve));
    }
    assert.deepStrictEqual(hostMessages, ['from a', { from: 'worker' }]);

    // Closed channels, and those of green threads that exited, receive
    // nothing.
    a.postMessage('close');
    assert.deepStrictEqual(await next(a), { type: 'closed' });
    a.on('message', common.mustNotCall());
    await b.terminate();
    host.postMessage('after close');
    const c = await relay(name, { broadcastScopes: [name] });
    host.postMessage('only c');
    assert.deepStrictEqual(await next(c),
                           { type: 'message', data: 'only c' });
    await new Promise((resolve) => setTimeout(resolve, 50));
    await a.terminate();
    await c.terminate();
    host.close();
  }

  // Messages that a green thread can't receive are 'messageerror' events, and
  // memory limits apply.
  {
    const name = 'limited';
    const host = new BroadcastChannel(name);
    const greenThread = await relay(name, {
      broadcastScopes: [name],
      resourceLimits: { maxMemoryMb: 16 },
    });
    host.postMessage(
      new WebAssembly.Module(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0])));
    assert.deepStrictEqual(await next(greenThread),
                           { type: 'messageerror', name: 'DataCloneError' });
    host.postMessage(new ArrayBuffer(64 * 1024 * 1024));
    assert.strictEqual((await next(greenThread)).type, 'messageerror');
    host.postMessage('small');
    assert.deepStrictEqual(await next(greenThread),
                           { type: 'message', data: 'small' });
    await greenThread.terminate();
    host.close();
  }
})().then(common.mustCall());
