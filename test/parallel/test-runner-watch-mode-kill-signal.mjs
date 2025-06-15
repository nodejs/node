import * as common from '../common/index.mjs';
import { describe, it, beforeEach } from 'node:test';
import { once } from 'node:events';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';

if (common.isWindows) {
  common.skip('no signals on Windows');
}

if (common.isIBMi) {
  common.skip('IBMi does not support `fs.watch()`');
}

if (common.isAIX) {
  common.skip('folder watch capability is limited in AIX.');
}

const indexContents = `
  const { setTimeout } = require("timers/promises");
  (async () => {
      // Wait a few milliseconds to make sure that the
      // parent process has time to attach its listeners
      await setTimeout(200);

      process.on('SIGTERM', () => {
          console.log('__SIGTERM received__');
          process.exit(123);
      });

      process.on('SIGINT', () => {
          console.log('__SIGINT received__');
          process.exit(124);
      });

      console.log('ready!');

      // Wait for a long time (just to keep the process alive)
      await setTimeout(100_000_000);
  })();
`;

let indexPath = '';

function refresh() {
  tmpdir.refresh();
  indexPath = tmpdir.resolve('index.js');
  writeFileSync(indexPath, indexContents);
}

describe('test runner watch mode with --watch-kill-signal', () => {
  beforeEach(refresh);

  it('defaults to SIGTERM', async () => {
    let currentRun = Promise.withResolvers();
    const child = spawn(process.execPath, ['--watch', indexPath], {
      cwd: tmpdir.path,
    });

    let stdout = '';
    child.stdout.on('data', (data) => {
      stdout += data.toString();
      currentRun.resolve();
    });

    await currentRun.promise;

    currentRun = Promise.withResolvers();
    writeFileSync(indexPath, indexContents);

    await currentRun.promise;
    child.kill();
    const [exitCode] = await once(child, 'exit');
    assert.match(stdout, /__SIGTERM received__/);
    assert.strictEqual(exitCode, 123);
  });

  it('can be overridden (to SIGINT)', async () => {
    let currentRun = Promise.withResolvers();
    const child = spawn(process.execPath, ['--watch', '--watch-kill-signal', 'SIGINT', indexPath], {
      cwd: tmpdir.path,
    });
    let stdout = '';

    child.stdout.on('data', (data) => {
      stdout += data.toString();
      if (stdout.includes('ready!')) {
        currentRun.resolve();
      }
    });

    await currentRun.promise;

    currentRun = Promise.withResolvers();
    writeFileSync(indexPath, indexContents);

    await currentRun.promise;
    child.kill();
    const [exitCode] = await once(child, 'exit');
    assert.match(stdout, /__SIGINT received__/);
    assert.strictEqual(exitCode, 124);
  });

  it('errors if an invalid signal is provided', async () => {
    const currentRun = Promise.withResolvers();
    const child = spawn(process.execPath, ['--watch', '--watch-kill-signal', 'invalid_signal', indexPath], {
      cwd: tmpdir.path,
    });
    let stdout = '';

    child.stderr.on('data', (data) => {
      stdout += data.toString();
      currentRun.resolve();
    });

    await currentRun.promise;

    assert.match(stdout, new RegExp(/TypeError \[ERR_UNKNOWN_SIGNAL\]: Unknown signal: invalid_signal/));
  });
});
