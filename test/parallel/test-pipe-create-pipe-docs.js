'use strict';
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { test } = require('node:test');
const { text } = require('node:stream/consumers');

// Keep these examples in sync with the pipe.createPipe() examples in
// doc/api/pipe.md.
const createPipeCjsExample = `
  const { spawn } = require('node:child_process');
  const { createPipe } = require('node:pipe');
  const { text } = require('node:stream/consumers');

  const { readable, writable } = createPipe();
  const child = spawn(process.execPath, ['-e', \`
    const fs = require('node:fs');
    const buffer = Buffer.alloc(1);
    const count = fs.readSync(0, buffer, 0, 1, null);
    fs.writeSync(1, buffer.subarray(0, count));
  \`], {
    stdio: [readable, 'pipe', 'inherit'],
  });

  const output = text(child.stdout);
  writable.end('abc');

  child.on('close', async () => {
    console.log(await output);
    console.log(await text(readable));
  });
`;

const createPipeMjsExample = `
  import { spawn } from 'node:child_process';
  import { createPipe } from 'node:pipe';
  import { text } from 'node:stream/consumers';

  const { readable, writable } = createPipe();
  const child = spawn(process.execPath, ['-e', \`
    const fs = require('node:fs');
    const buffer = Buffer.alloc(1);
    const count = fs.readSync(0, buffer, 0, 1, null);
    fs.writeSync(1, buffer.subarray(0, count));
  \`], {
    stdio: [readable, 'pipe', 'inherit'],
  });

  const output = text(child.stdout);
  writable.end('abc');

  child.on('close', async () => {
    console.log(await output);
    console.log(await text(readable));
  });
`;

async function waitForClose(child) {
  const [code, signal] = await once(child, 'close');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

async function runExample(args, code) {
  const child = spawn(process.execPath, [...args, code], {
    stdio: ['ignore', 'pipe', 'inherit'],
  });
  const output = text(child.stdout);

  await waitForClose(child);
  assert.strictEqual(await output, 'a\nbc\n');
}

test('pipe.createPipe documentation examples', async () => {
  await runExample(['-e'], createPipeCjsExample);
  await runExample(['--input-type=module', '-e'], createPipeMjsExample);
});
