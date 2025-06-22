import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import path from 'node:path';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';
import { spawn } from 'node:child_process';
import { writeFileSync, mkdirSync } from 'node:fs';
import { inspect } from 'node:util';
import { createInterface } from 'node:readline';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

let tmpFiles = 0;
function createTmpFile(content, ext, basename) {
  const file = path.join(basename, `${tmpFiles++}${ext}`);
  writeFileSync(file, content);
  return file;
}

async function runNode({
  args,
  expectedCompletionLog = 'Completed running',
  options = {},
}) {
  const child = spawn(execPath, args, { encoding: 'utf8', stdio: 'pipe', ...options });
  let stderr = '';
  const stdout = [];

  child.stderr.on('data', (data) => {
    stderr += data;
  });

  try {
    // Break the chunks into lines
    for await (const data of createInterface({ input: child.stdout })) {
      if (!data.startsWith('Waiting for graceful termination') && !data.startsWith('Gracefully restarted')) {
        stdout.push(data);
      }
      if (data.startsWith(expectedCompletionLog)) {
        break;
      }
    }
  } finally {
    child.kill();
  }
  return { stdout, stderr, pid: child.pid };
}

tmpdir.refresh();

describe('watch mode - watch flags', { concurrency: !process.env.TEST_PARALLEL, timeout: 60_000 }, () => {
  it('when multiple `--watch` flags are provided should run as if only one was', async () => {
    const projectDir = tmpdir.resolve('project-multi-flag');
    mkdirSync(projectDir);

    const file = createTmpFile(`
      console.log(
        process.argv.some(arg => arg === '--watch')
        ? 'Error: unexpected --watch args present'
        : 'no --watch args present'
      );`, '.js', projectDir);
    const args = ['--watch', '--watch', file];
    const { stdout, stderr } = await runNode({
      file, args, options: { cwd: projectDir }
    });

    assert.strictEqual(stderr, '');
    assert.deepStrictEqual(stdout, [
      'no --watch args present',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
    ]);
  });

  it('`--watch-path` args without `=` used alongside `--watch` should not make it into the script', async () => {
    const projectDir = tmpdir.resolve('project-watch-watch-path-args');
    mkdirSync(projectDir);

    const file = createTmpFile(`
      console.log(
        process.argv.slice(2).some(arg => arg.endsWith('.js'))
        ? 'some cli args end with .js'
        : 'no cli arg ends with .js'
      );`, '.js', projectDir);
    const args = ['--watch', `--watch-path`, file, file];
    const { stdout, stderr } = await runNode({
      file, args, options: { cwd: projectDir }
    });

    assert.strictEqual(stderr, '');
    assert.deepStrictEqual(stdout, [
      'no cli arg ends with .js',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
    ]);
  });
});
