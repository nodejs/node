// Flags: --experimental-green-threads --no-warnings
'use strict';

// Transferring ArrayBuffers to and from green threads, and sharing
// SharedArrayBuffers with them.

const common = require('../common');
const assert = require('assert');
const { GreenThread } = require('node:green_threads');

function nextMessage(greenThread) {
  return new Promise((resolve, reject) => {
    greenThread.once('message', resolve);
    greenThread.once('error', reject);
  });
}

(async () => {
  // In both directions, as an array or with { transfer }.
  {
    const greenThread = new GreenThread(`
      onmessage = ({ data: { buffer, view } }) => {
        const back = new ArrayBuffer(4);
        new Uint8Array(back)[3] = 9;
        const result = [buffer.byteLength, new Uint8Array(buffer)[0],
                        view.byteLength, view[1]];
        postMessage({ result, back }, [back]);
        postMessage(back.byteLength);
      };
    `);
    const buffer = new ArrayBuffer(8);
    new Uint8Array(buffer)[0] = 7;
    const view = new Uint8Array(new ArrayBuffer(2));
    view[1] = 3;
    greenThread.postMessage({ buffer, view }, { transfer: [buffer, view.buffer] });
    assert.strictEqual(buffer.byteLength, 0);
    assert.strictEqual(view.byteLength, 0);
    const { result, back } = await nextMessage(greenThread);
    assert.deepStrictEqual(result, [8, 7, 2, 3]);
    assert.strictEqual(new Uint8Array(back)[3], 9);
    assert.strictEqual(await nextMessage(greenThread), 0);
    await greenThread.terminate();
  }

  // Transfer lists that can't be transferred. Nothing is detached then.
  {
    const greenThread = new GreenThread(`
      const results = [];
      const buffer = new ArrayBuffer(1);
      for (const transfer of [[{}], [buffer, buffer], 'x',
                              [new SharedArrayBuffer(1)]]) {
        try {
          postMessage(0, transfer);
        } catch (error) {
          results.push(error.name);
        }
      }
      results.push(buffer.byteLength);
      postMessage(results);
    `);
    assert.deepStrictEqual(await nextMessage(greenThread), [
      'DataCloneError', 'DataCloneError', 'DataCloneError', 'DataCloneError',
      1,
    ]);
    const buffer = new ArrayBuffer(1);
    assert.throws(() => greenThread.postMessage(buffer, [buffer, buffer]), {
      name: 'DataCloneError',
    });
    assert.strictEqual(buffer.byteLength, 1);
    await greenThread.terminate();
  }

  // SharedArrayBuffers are shared. Atomics.wait() would block a pool thread.
  {
    const shared = new SharedArrayBuffer(4);
    const greenThread = new GreenThread(`
      onmessage = ({ data: shared }) => {
        const array = new Int32Array(shared);
        Atomics.add(array, 0, 5);
        try {
          Atomics.wait(array, 0, 5, 1);
        } catch (error) {
          postMessage(error.name);
        }
      };
    `);
    greenThread.postMessage(shared);
    assert.strictEqual(await nextMessage(greenThread), 'TypeError');
    assert.strictEqual(new Int32Array(shared)[0], 5);
    await greenThread.terminate();
  }

  // With a memory limit, buffers that are transferred away no longer count
  // towards it, and those that are transferred in do.
  {
    const greenThread = new GreenThread(`
      onmessage = ({ data }) => {
        if (data instanceof ArrayBuffer) {
          postMessage(data.byteLength);
          return;
        }
        for (let i = 0; i < 10; i++) {
          const buffer = new ArrayBuffer(20 * 1024 * 1024);
          new Uint8Array(buffer)[0] = i;
          postMessage(buffer, [buffer]);
        }
        postMessage('sent');
      };
    `, { resourceLimits: { maxMemoryMb: 32 } });
    const received = [];
    greenThread.on('message', (message) => {
      if (message instanceof ArrayBuffer) {
        received.push(new Uint8Array(message)[0]);
      }
    });
    greenThread.postMessage(0);
    await new Promise((resolve) => {
      greenThread.on('message', (message) => {
        if (message === 'sent') resolve();
      });
    });
    assert.deepStrictEqual(received, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);

    const small = new ArrayBuffer(8 * 1024 * 1024);
    greenThread.postMessage(small, [small]);
    await new Promise((resolve) => {
      greenThread.on('message', (message) => {
        if (message === 8 * 1024 * 1024) resolve();
      });
    });

    const big = new ArrayBuffer(64 * 1024 * 1024);
    greenThread.postMessage(big, [big]);
    const error = await new Promise((resolve) => {
      greenThread.once('error', resolve);
    });
    assert.strictEqual(error.name, 'RangeError');
    assert.strictEqual(await new Promise((resolve) => {
      greenThread.once('exit', resolve);
    }), 1);
  }
})().then(common.mustCall());
