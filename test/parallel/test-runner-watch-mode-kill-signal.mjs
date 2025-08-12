import * as common from '../common/index.mjs';
import { describe, it, beforeEach } from 'node:test';
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
  process.on('SIGTERM', () => { console.log('__SIGTERM received__'); process.exit(); });
  process.on('SIGINT', () => { console.log('__SIGINT received__'); process.exit(); });
  process.send('script ready');
  setTimeout(() => {}, 100_000);
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
    const child = spawn(
      process.execPath,
      ['--watch', indexPath],
      {
        cwd: tmpdir.path,
        stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
      }
    );

    let stdout = '';
    child.stdout.on('data', (data) => {
      stdout += `${data}`;
      if (/__(SIGINT|SIGTERM) received__/.test(stdout)) {
        child.kill();
      }
    });

    child.on('message', (msg) => {
      if (msg === 'script ready') {
        writeFileSync(indexPath, indexContents);
      }
    });

    await new Promise((resolve) =>
      child.on('exit', () => {
        resolve();
      })
    );

    assert.match(stdout, /__SIGTERM received__/);
    assert.doesNotMatch(stdout, /__SIGINT received__/);
  });

  it('can be overridden (to SIGINT)', async () => {
    const child = spawn(
      process.execPath,
      ['--watch', '--watch-kill-signal', 'SIGINT', indexPath],
      {
        cwd: tmpdir.path,
        stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
      }
    );

    let stdout = '';
    child.stdout.on('data', (data) => {
      stdout += `${data}`;
      if (/__(SIGINT|SIGTERM) received__/.test(stdout)) {
        child.kill();
      }
    });

    child.on('message', (msg) => {
      if (msg === 'script ready') {
        writeFileSync(indexPath, indexContents);
      }
    });

    await new Promise((resolve) =>
      child.on('exit', () => {
        resolve();
      })
    );

    assert.match(stdout, /__SIGINT received__/);
    assert.doesNotMatch(stdout, /__SIGTERM received__/);
  });

  it('errors if an invalid signal is provided', async () => {
    const currentRun = Promise.withResolvers();
    const child = spawn(
      process.execPath,
      ['--watch', '--watch-kill-signal', 'invalid_signal', indexPath],
      {
        cwd: tmpdir.path,
      }
    );
    let stdout = '';

    child.stderr.on('data', (data) => {
      stdout += data.toString();
      currentRun.resolve();
    });

    await currentRun.promise;

    assert.match(
      stdout,
      new RegExp(
        /TypeError \[ERR_UNKNOWN_SIGNAL\]: Unknown signal: invalid_signal/
      )
    );
  });
});
