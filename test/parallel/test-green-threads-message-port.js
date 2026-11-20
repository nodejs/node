// Flags: --experimental-green-threads --no-warnings
'use strict';

// MessageChannel and MessagePort in green threads, and passing ports between
// green threads, their host and workers.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { BlockList } = require('net');
const { GreenThread } = require('node:green_threads');
const { MessageChannel, Worker } = require('worker_threads');

function nextMessage(target) {
  return new Promise((resolve, reject) => {
    target.once('message', resolve);
    target.once('error', reject);
  });
}

(async () => {
  // Inside a green thread. Messages wait until the port starts, and both
  // ports emit 'close'.
  {
    const greenThread = new GreenThread(`
      const { port1, port2 } = new MessageChannel();
      const events = [];
      const buffer = new ArrayBuffer(4);
      port1.postMessage({ buffer }, [buffer]);
      port1.postMessage(2);
      events.push(buffer.byteLength, String(port1), String(new MessageChannel()));
      setTimeout(() => {
        port2.onmessage = (event) => {
          events.push(event instanceof MessageEvent, event.isTrusted,
                      event.data.buffer?.byteLength ?? event.data);
          if (event.data === 2) port2.close();
        };
      }, 10);
      let closed = 0;
      const onclose = () => {
        if (++closed === 2) postMessage(events);
      };
      port1.addEventListener('close', onclose);
      port2.addEventListener('close', onclose);
    `);
    assert.deepStrictEqual(await nextMessage(greenThread), [
      0, '[object MessagePort]', '[object MessageChannel]',
      true, true, 4,
      true, true, 2,
    ]);
    await greenThread.terminate();
  }

  // Ports of the host's MessageChannels, in both directions.
  {
    const greenThread = new GreenThread(`
      onmessage = ({ data, ports }) => {
        if (data === 'echo') {
          ports[0].onmessage = ({ data }) => ports[0].postMessage(data * 2);
          return;
        }
        const { port1, port2 } = new MessageChannel();
        port1.onmessage = ({ data }) => postMessage(['received', data]);
        postMessage({ port: port2 }, [port2]);
      };
    `);
    const { port1, port2 } = new MessageChannel();
    greenThread.postMessage('echo', [port2]);
    port1.postMessage(21);
    assert.strictEqual(await nextMessage(port1), 42);
    port1.close();

    greenThread.postMessage('channel');
    const { port } = await nextMessage(greenThread);
    port.postMessage('hello');
    assert.deepStrictEqual(await nextMessage(greenThread),
                           ['received', 'hello']);

    // Ports close when their green thread exits.
    const closed = once(port, 'close');
    await greenThread.terminate();
    await closed;
  }

  // A worker and a green thread, connected by a port.
  {
    const { port1, port2 } = new MessageChannel();
    const worker = new Worker(`
      const { workerData: { port } } = require('worker_threads');
      port.on('message', (message) => {
        port.postMessage({ message, shared: message.shared });
        port.close();
      });
    `, { eval: true, workerData: { port: port1 }, transferList: [port1] });
    const workerExited = once(worker, 'exit');
    const greenThread = new GreenThread(`
      onmessage = ({ data: port }) => {
        const shared = new SharedArrayBuffer(4);
        port.onmessage = ({ data }) => {
          new Int32Array(data.shared)[0] = 7;
          postMessage([data.message.text, new Int32Array(shared)[0]]);
        };
        port.addEventListener('close', () => postMessage('closed'));
        port.postMessage({ text: 'hi', shared });
      };
    `);
    greenThread.postMessage(port2, [port2]);
    assert.deepStrictEqual(await nextMessage(greenThread), ['hi', 7]);
    assert.strictEqual(await nextMessage(greenThread), 'closed');
    await Promise.all([greenThread.terminate(), workerExited]);
  }

  // A port that moves from one green thread to another through the host.
  {
    const first = new GreenThread(`
      const { port1, port2 } = new MessageChannel();
      port1.onmessage = ({ data }) => postMessage(data);
      postMessage(port2, [port2]);
    `);
    const second = new GreenThread(`
      onmessage = ({ data: port }) => port.postMessage('from the second');
    `);
    const port = await nextMessage(first);
    second.postMessage(port, [port]);
    assert.strictEqual(await nextMessage(first), 'from the second');
    await Promise.all([first.terminate(), second.terminate()]);
  }

  // Transfer lists that can't be transferred, and ports that are not in the
  // transfer list.
  {
    const greenThread = new GreenThread(`
      const errors = [];
      const { port1, port2 } = new MessageChannel();
      for (const [value, transfer] of [
        [port2, []],
        [0, [port1]],
        [0, [port2, port2]],
      ]) {
        try {
          port1.postMessage(value, transfer);
        } catch (error) {
          errors.push(error.name, error.message);
        }
      }
      const { port1: other } = new MessageChannel();
      postMessage(other, [other]);
      try {
        postMessage(0, [other]);
      } catch (error) {
        errors.push(error.name, error.message);
      }
      // Posting on a transferred port drops the message.
      other.postMessage(0);
      postMessage(errors);
    `);
    await nextMessage(greenThread);
    assert.deepStrictEqual(await nextMessage(greenThread), [
      'DataCloneError', 'A MessagePort must be in the transfer list to be sent',
      'DataCloneError', 'Transfer list contains source port',
      'DataCloneError', 'A MessagePort is in the transfer list more than once',
      'DataCloneError', 'MessagePort in transfer list is already detached',
    ]);
    const { port1, port2 } = new MessageChannel();
    assert.throws(() => greenThread.postMessage(port2), {
      name: 'DataCloneError',
    });
    port1.close();
    await greenThread.terminate();
  }

  // What Node.js can send that green threads can't receive is a
  // 'messageerror'. process.env is a plain object.
  {
    const greenThread = new GreenThread(`
      onmessage = ({ ports: [port] }) => {
        port.onmessage = ({ data }) => postMessage(['message', typeof data.PATH]);
        port.onmessageerror = ({ data }) => postMessage(['messageerror', data.name]);
      };
    `);
    const { port1, port2 } = new MessageChannel();
    greenThread.postMessage(0, [port2]);
    port1.postMessage(new BlockList());
    assert.deepStrictEqual(await nextMessage(greenThread),
                           ['messageerror', 'DataCloneError']);
    port1.postMessage(process.env);
    assert.deepStrictEqual(await nextMessage(greenThread),
                           ['message', typeof process.env.PATH]);
    port1.close();
    await greenThread.terminate();
  }

  // MessagePorts can't be constructed.
  {
    const greenThread = new GreenThread(`
      try {
        new MessagePort();
      } catch (error) {
        postMessage(error.name);
      }
    `);
    assert.strictEqual(await nextMessage(greenThread), 'TypeError');
    await greenThread.terminate();
  }
})().then(common.mustCall());
