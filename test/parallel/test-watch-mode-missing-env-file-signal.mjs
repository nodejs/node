import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { writeFileSync } from 'node:fs';
import { clearTimeout, setTimeout } from 'node:timers';
import tmpdir from '../common/tmpdir.js';

if (!common.isLinux)
  common.skip('This test verifies Linux fs.watch() behavior');

tmpdir.refresh();
const entry = tmpdir.resolve('entry.js');
const missingEnvFile = tmpdir.resolve('missing.env');
writeFileSync(entry, '');

async function withTimeout(promise, message) {
  let timeout;
  const timedOut = new Promise((_, reject) => {
    timeout = setTimeout(
      () => reject(new Error(message)),
      common.platformTimeout(10_000),
    );
  });

  try {
    return await Promise.race([promise, timedOut]);
  } finally {
    clearTimeout(timeout);
  }
}

async function testSignal(signal) {
  const child = spawn(process.execPath, [
    '--watch',
    `--env-file-if-exists=${missingEnvFile}`,
    entry,
  ], {
    cwd: tmpdir.path,
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  const closed = once(child, 'close');
  const ready = Promise.withResolvers();
  let stdout = '';
  let stderr = '';

  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => {
    stdout += data;
    if (stdout.includes('Completed running')) {
      ready.resolve();
    }
  });
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.once('error', ready.reject);
  child.once('exit', (code, exitSignal) => {
    ready.reject(new Error(
      `Watch mode exited before becoming ready: code=${code}, signal=${exitSignal}`,
    ));
  });

  try {
    await withTimeout(
      ready.promise,
      'Timed out waiting for watch mode readiness',
    );
    assert.strictEqual(child.kill(signal), true);
    const [code, exitSignal] = await withTimeout(
      closed,
      `Timed out waiting for watch mode to exit after ${signal}`,
    );

    assert.strictEqual(code, 0);
    assert.strictEqual(exitSignal, null);
    assert.doesNotMatch(
      `${stdout}\n${stderr}`,
      /Assertion failed|FSEventWrap::GetInitialized/,
    );
  } finally {
    if (child.exitCode === null && child.signalCode === null) {
      child.kill('SIGKILL');
    }
    await closed.catch(() => {});
  }
}

await testSignal('SIGINT');
await testSignal('SIGTERM');
