'use strict';
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { test } = require('node:test');
const { text } = require('node:stream/consumers');

// Keep these examples in sync with the net.createSocketPair() examples in
// doc/api/net.md.
const createSocketPairCjsExample = `
  const { spawn } = require('node:child_process');
  const { createSocketPair } = require('node:net');

  const [left, right] = createSocketPair();

  const child = spawn(process.execPath, ['-e', \`
    process.on('message', (message, socket) => {
      socket.on('data', (chunk) => {
        socket.write(chunk.toString().toUpperCase());
      });
      socket.resume();
      process.send('ready');
    });
  \`], {
    stdio: ['ignore', 'inherit', 'inherit', 'ipc'],
  });

  child.once('message', () => {
    left.write('hello');
  });

  left.once('data', (chunk) => {
    console.log(chunk.toString()); // Prints: HELLO
    left.destroy();
    child.kill();
  });

  child.send('socket', right, { keepOpen: false });
`;

const createSocketPairMjsExample = `
  import { spawn } from 'node:child_process';
  import { createSocketPair } from 'node:net';

  const [left, right] = createSocketPair();

  const child = spawn(process.execPath, ['-e', \`
    process.on('message', (message, socket) => {
      socket.on('data', (chunk) => {
        socket.write(chunk.toString().toUpperCase());
      });
      socket.resume();
      process.send('ready');
    });
  \`], {
    stdio: ['ignore', 'inherit', 'inherit', 'ipc'],
  });

  child.once('message', () => {
    left.write('hello');
  });

  left.once('data', (chunk) => {
    console.log(chunk.toString()); // Prints: HELLO
    left.destroy();
    child.kill();
  });

  child.send('socket', right, { keepOpen: false });
`;

async function waitForClose(child) {
  const [code, signal] = await once(child, 'close');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

async function runExample(args, code, expected) {
  const child = spawn(process.execPath, [...args, code], {
    stdio: ['ignore', 'pipe', 'inherit'],
  });
  const output = text(child.stdout);

  await waitForClose(child);
  assert.strictEqual(await output, expected);
}

test('net.createSocketPair documentation examples', async () => {
  await runExample(['-e'], createSocketPairCjsExample, 'HELLO\n');
  await runExample(['--input-type=module', '-e'],
                   createSocketPairMjsExample,
                   'HELLO\n');
});
