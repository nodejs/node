'use strict';
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { createPipe } = require('node:net');
const { test } = require('node:test');
const { text } = require('node:stream/consumers');

const pipeStdinToStdout = `
  process.stdin.pipe(process.stdout);
`;

const pipeFd3ToStdout = `
  const fs = require('node:fs');
  fs.createReadStream(null, { fd: 3 }).pipe(process.stdout);
`;

const writeStdout = `
  process.stdout.write('hello stdout\\n');
`;

const writeStderr = `
  process.stderr.write('hello stderr\\n');
`;

const writeFd3 = `
  const fs = require('node:fs');
  fs.writeSync(3, 'hello fd3\\n');
`;

async function waitForClose(child) {
  const [code, signal] = await once(child, 'close');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

test('parent-owned streams do not appear as stdin', () => {
  const { readable, writable } = createPipe();

  const child = spawn(process.execPath, ['-e', pipeStdinToStdout], {
    stdio: [readable, 'pipe', 'inherit'],
  });
  assert.strictEqual(child.stdin, null);

  child.kill();
  readable.destroy();
  writable.destroy();
});

test('parent-owned streams do not appear as stdout', () => {
  const { readable, writable } = createPipe();

  const child = spawn(process.execPath, ['-e', pipeStdinToStdout], {
    stdio: ['pipe', writable, 'inherit'],
  });
  assert.strictEqual(child.stdout, null);

  child.kill();
  readable.destroy();
  writable.destroy();
});

test('parent-owned streams do not appear as stderr', () => {
  const { readable, writable } = createPipe();

  const child = spawn(process.execPath, ['-e', pipeStdinToStdout], {
    stdio: ['pipe', 'pipe', writable],
  });
  assert.strictEqual(child.stderr, null);

  child.kill();
  readable.destroy();
  writable.destroy();
});

test('child stdin leases readable end and pipes to stdout', async () => {
  const { readable, writable } = createPipe();

  const child = spawn(process.execPath, ['-e', pipeStdinToStdout], {
    stdio: [readable, 'pipe', 'inherit'],
  });
  const output = text(child.stdout);

  writable.end('abc');
  await waitForClose(child);
  assert.strictEqual(await output, 'abc');
});

test('fd 3 leases readable end and pipes to stdout', async () => {
  const { readable, writable } = createPipe();

  const child = spawn(process.execPath, ['-e', pipeFd3ToStdout], {
    stdio: ['ignore', 'pipe', 'inherit', readable],
  });
  const output = text(child.stdout);

  writable.end('abc');
  await waitForClose(child);
  assert.strictEqual(await output, 'abc');
});

test('child stdout leases writable end and parent reads output', async () => {
  const { readable, writable } = createPipe();
  const child = spawn(process.execPath, ['-e', writeStdout], {
    stdio: ['ignore', writable, 'inherit'],
  });

  writable.destroy();
  await waitForClose(child);

  assert.strictEqual(await text(readable), 'hello stdout\n');
});

test('child stderr leases writable end and parent reads output', async () => {
  const { readable, writable } = createPipe();
  const child = spawn(process.execPath, ['-e', writeStderr], {
    stdio: ['ignore', 'ignore', writable],
  });

  writable.destroy();

  assert.strictEqual(await text(readable), 'hello stderr\n');
  await waitForClose(child);
});

test('fd 3 leases writable end and parent reads output', async () => {
  const { readable, writable } = createPipe();
  const child = spawn(process.execPath, ['-e', writeFd3], {
    stdio: ['ignore', 'ignore', 'inherit', writable],
  });

  writable.destroy();

  assert.strictEqual(await text(readable), 'hello fd3\n');
  await waitForClose(child);
});

test('parent-owned readable does not flow after child exits',
  async () => {
    const { readable, writable } = createPipe();
    let flowed = '';
    readable.on('data', (chunk) => {
      flowed += chunk;
    });
    readable.pause();

    const child = spawn(process.execPath, ['-e', writeStdout], {
      stdio: ['ignore', writable, 'inherit'],
    });

    writable.destroy();
    await waitForClose(child);

    assert.strictEqual(flowed, '');
    assert.strictEqual(readable.readableFlowing, false);
    assert.strictEqual(await text(readable), 'hello stdout\n');
  });

test('parent-owned pipes must be explicitly closed in a pipeline', async () => {
  const { readable, writable } = createPipe();
  const producer = spawn(process.execPath, [
    '-e',
    "process.stdout.write('a')",
  ], {
    stdio: ['ignore', writable, 'inherit'],
  });
  const consumer = spawn(process.execPath, ['-e', pipeStdinToStdout], {
    stdio: [readable, 'pipe', 'inherit'],
  });
  const output = text(consumer.stdout);

  await waitForClose(producer);

  // The parent owns the pipe endpoints. Close the writable endpoint so the
  // consumer can observe EOF and close. This is a degenerate case. A 'pipe'
  // should be used instead of a parent-owned pipe to form a pipeline.
  writable.destroy();
  await waitForClose(consumer);
  assert.strictEqual(await output, 'a');
});
