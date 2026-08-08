'use strict';
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { createPipe } = require('node:net');
const { test } = require('node:test');
const { text } = require('node:stream/consumers');

const readOneByteFromStdin = `
  const fs = require('node:fs');
  const buffer = Buffer.alloc(1);
  const count = fs.readSync(0, buffer, 0, 1, null);
  fs.writeSync(1, buffer.subarray(0, count));
`;

const writeArgToStdout = `
  process.stdout.write(process.argv[1]);
`;

async function waitForClose(child) {
  const [code, signal] = await once(child, 'close');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

test('parent can lease readable to children sequentially then reclaim', 
  async () => {
    const { readable, writable } = createPipe();

    writable.write('abc');

    // child A consumes 'a'
    const childA = spawn(process.execPath, ['-e', readOneByteFromStdin], {
      stdio: [readable, 'pipe', 'inherit'],
    });
    const outputA = text(childA.stdout);

    await waitForClose(childA);
    assert.strictEqual(await outputA, 'a');

    // child B consumes 'b'
    const childB = spawn(process.execPath, ['-e', readOneByteFromStdin], {
      stdio: [readable, 'pipe', 'inherit'],
    });
    const outputB = text(childB.stdout);

    await waitForClose(childB);
    assert.strictEqual(await outputB, 'b');

    // parent consumes 'c'
    writable.end();
    assert.strictEqual(await text(readable), 'c');
  });

test('parent can lease writable to children sequentially then write',
  async () => {
    const { readable, writable } = createPipe();

    // child A writes 'a'
    const childA = spawn(process.execPath, ['-e', writeArgToStdout, 'a'], {
      stdio: ['ignore', writable, 'inherit'],
    });
    await waitForClose(childA);

    // child B writes 'b'
    const childB = spawn(process.execPath, ['-e', writeArgToStdout, 'b'], {
      stdio: ['ignore', writable, 'inherit'],
    });
    await waitForClose(childB);

    // parent writes 'c'
    const output = text(readable);
    writable.end('c');
    assert.strictEqual(await output, 'abc');
  });

