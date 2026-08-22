'use strict';
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { createSocketPair } = require('node:net');
const { test } = require('node:test');

const echoUppercase = `
  process.on('message', (message, socket) => {
    if (message === 'done') {
      process.exit(0);
      return;
    }

    socket.once('data', (chunk) => {
      socket.write(chunk.toString().toUpperCase());
      process.send('wrote');
    });
    socket.resume();
    process.send('ready');
  });
`;

async function waitForClose(child) {
  const [code, signal] = await once(child, 'close');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

function sendHandle(child, handle) {
  return new Promise((resolve, reject) => {
    child.send('socket', handle, { keepOpen: true }, (error) => {
      if (error) reject(error);
      else resolve();
    });
  });
}

function tick() {
  return new Promise((resolve) => setImmediate(resolve));
}

test('socket pair endpoint can be sent to a child process', async () => {
  const [left, right] = createSocketPair();
  const child = spawn(process.execPath, ['-e', echoUppercase], {
    stdio: ['ignore', 'inherit', 'inherit', 'ipc'],
  });

  try {
    left.on('error', (error) => {
      assert.strictEqual(error.code, 'ECONNRESET');
    });
    const ready = once(child, 'message');
    await sendHandle(child, right);
    assert.deepStrictEqual(await ready, ['ready', undefined]);
    await tick();
    const data = once(left, 'data');
    const wrote = once(child, 'message');
    const close = waitForClose(child);
    left.write('hello child');

    assert.strictEqual((await data)[0].toString(), 'HELLO CHILD');
    assert.deepStrictEqual(await wrote, ['wrote', undefined]);
    child.send('done');
    await close;
  } finally {
    child.kill();
    left.destroy();
    right.destroy();
  }
});
