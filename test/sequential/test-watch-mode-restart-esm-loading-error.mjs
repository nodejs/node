import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import path from 'node:path';
import { execPath } from 'node:process';
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import { inspect } from 'node:util';
import { createInterface } from 'node:readline';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

if (common.isAIX)
  common.skip('AIX does not reliably capture syntax errors in watch mode');

let tmpFiles = 0;
function createTmpFile(content = 'console.log("running");', ext = '.js', basename = tmpdir.path) {
  const file = path.join(basename, `${tmpFiles++}${ext}`);
  writeFileSync(file, content);
  return file;
}

function runInBackground({ args = [], options = {}, completed = 'Completed running', shouldFail = false }) {
  let future = Promise.withResolvers();
  let child;
  let stderr = '';
  let stdout = [];

  const run = () => {
    args.unshift('--no-warnings');
    child = spawn(execPath, args, { encoding: 'utf8', stdio: 'pipe', ...options });

    child.stderr.on('data', (data) => {
      stderr += data;
    });

    const rl = createInterface({ input: child.stdout });
    rl.on('line', (data) => {
      if (!data.startsWith('Waiting for graceful termination') && !data.startsWith('Gracefully restarted')) {
        stdout.push(data);
        if (data.startsWith(completed)) {
          future.resolve({ stderr, stdout });
          future = Promise.withResolvers();
          stdout = [];
          stderr = '';
        } else if (data.startsWith('Failed running')) {
          const settle = () => {
            if (shouldFail) {
              future.resolve({ stderr, stdout });
            } else {
              future.reject({ stderr, stdout });
            }
            future = Promise.withResolvers();
            stdout = [];
            stderr = '';
          };
          // If stderr is empty, wait for it to receive data before settling.
          // This handles the race condition where stdout arrives before stderr.
          if (stderr === '') {
            child.stderr.once('data', settle);
          } else {
            settle();
          }
        }
      }
    });
  };

  return {
    async done() {
      child?.kill();
      future.resolve();
      return { stdout, stderr };
    },
    restart(timeout = 1000) {
      if (!child) {
        run();
      }
      const timer = setTimeout(() => {
        if (!future.resolved) {
          child.kill();
          future.reject(new Error('Timed out waiting for restart'));
        }
      }, timeout);
      return future.promise.finally(() => {
        clearTimeout(timer);
      });
    },
  };
}

tmpdir.refresh();

// Create initial file with valid code
const initialContent = `console.log('hello, world');`;
const file = createTmpFile(initialContent, '.mjs');

const { done, restart } = runInBackground({
  args: ['--watch', file],
  completed: 'Completed running',
  shouldFail: true,
});

try {
  const { stdout, stderr } = await restart();
  assert.strictEqual(stderr, '');
  assert.deepStrictEqual(stdout, [
    'hello, world',
    `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
  ]);

  // Update file with syntax error
  const syntaxErrorContent = `console.log('hello, wor`;
  writeFileSync(file, syntaxErrorContent);

  // Wait for the failed restart
  const { stderr: stderr2, stdout: stdout2 } = await restart();
  assert.match(stderr2, /SyntaxError: Invalid or unexpected token/);
  assert.deepStrictEqual(stdout2, [
    `Restarting ${inspect(file)}`,
    `Failed running ${inspect(file)}. Waiting for file changes before restarting...`,
  ]);

  writeFileSync(file, `console.log('hello again, world');`);

  const { stderr: stderr3, stdout: stdout3 } = await restart();

  // Verify it recovered and ran successfully
  assert.strictEqual(stderr3, '');
  assert.deepStrictEqual(stdout3, [
    `Restarting ${inspect(file)}`,
    'hello again, world',
    `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
  ]);
} finally {
  await done();
}
