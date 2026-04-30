import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import path from 'node:path';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';
import { spawn } from 'node:child_process';
import { writeFileSync, readFileSync } from 'node:fs';
import { inspect } from 'node:util';
import { pathToFileURL } from 'node:url';
import { createInterface } from 'node:readline';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

function restart(file, content = readFileSync(file)) {
  writeFileSync(file, content);
  const timer = setInterval(() => writeFileSync(file, content), common.platformTimeout(250));
  return () => clearInterval(timer);
}

let tmpFiles = 0;
function createTmpFile(content = 'console.log(\'running\');', ext = '.js', basename = tmpdir.path) {
  const file = path.join(basename, `${tmpFiles++}${ext}`);
  writeFileSync(file, content);
  return file;
}

async function runWriteSucceed({
  file,
  watchedFile,
  watchFlag = '--watch',
  args = [file],
  completed = 'Completed running',
  restarts = 2,
  options = {},
  shouldFail = false,
}) {
  args.unshift('--no-warnings');
  if (watchFlag !== null) args.unshift(watchFlag);

  const child = spawn(execPath, args, { encoding: 'utf8', stdio: 'pipe', ...options });

  let completes = 0;
  let cancelRestarts = () => {};
  let stderr = '';
  const stdout = [];

  child.stderr.on('data', (data) => {
    stderr += data;
  });

  try {
    for await (const data of createInterface({ input: child.stdout })) {
      if (!data.startsWith('Waiting for graceful termination') &&
          !data.startsWith('Gracefully restarted')) {
        stdout.push(data);
      }

      if (data.startsWith(completed)) {
        completes++;

        if (completes === restarts) break;

        if (completes === 1) {
          cancelRestarts = restart(watchedFile);
        }
      }

      if (!shouldFail && data.startsWith('Failed running')) break;
    }
  } finally {
    child.kill();
    cancelRestarts();
  }

  return { stdout, stderr, pid: child.pid };
}

tmpdir.refresh();
const dir = tmpdir.path;

describe('watch mode', { concurrency: !process.env.TEST_PARALLEL, timeout: 60_000 }, () => {
  it('should watch changes to worker - cjs', async () => {
    const worker = path.join(dir, 'worker.js');

    writeFileSync(worker, `
console.log('worker running');
`);

    const file = createTmpFile(`
const { Worker } = require('node:worker_threads');
const w = new Worker(${JSON.stringify(worker)});
`, '.js', dir);

    const { stderr, stdout } = await runWriteSucceed({
      file,
      watchedFile: worker,
    });

    assert.strictEqual(stderr, '');
    assert.deepStrictEqual(stdout, [
      'worker running',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
      `Restarting ${inspect(file)}`,
      'worker running',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
    ]);
  });

  it('should watch changes to worker dependencies - cjs', async () => {
    const dep = path.join(dir, 'dep.js');
    const worker = path.join(dir, 'worker.js');

    writeFileSync(dep, `
module.exports = 'dep v1';
`);

    writeFileSync(worker, `
const dep = require('./dep.js');
console.log(dep);
`);

    const file = createTmpFile(`
const { Worker } = require('node:worker_threads');
const w = new Worker(${JSON.stringify(worker)});
`, '.js', dir);

    const { stderr, stdout } = await runWriteSucceed({
      file,
      watchedFile: dep,
    });

    assert.strictEqual(stderr, '');
    assert.deepStrictEqual(stdout, [
      'dep v1',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
      `Restarting ${inspect(file)}`,
      'dep v1',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
    ]);
  });

  it('should watch changes to nested worker dependencies - cjs', async () => {
    const subDep = path.join(dir, 'sub-dep.js');
    const dep = path.join(dir, 'dep.js');
    const worker = path.join(dir, 'worker.js');

    writeFileSync(subDep, `
module.exports = 'sub-dep v1';
`);

    writeFileSync(dep, `
const subDep = require('./sub-dep.js');
console.log(subDep);
module.exports = 'dep v1';
`);

    writeFileSync(worker, `
const dep = require('./dep.js');
`);

    const file = createTmpFile(`
const { Worker } = require('node:worker_threads');
const w = new Worker(${JSON.stringify(worker)});
`, '.js', dir);

    const { stderr, stdout } = await runWriteSucceed({
      file,
      watchedFile: subDep,
    });

    assert.strictEqual(stderr, '');
    assert.deepStrictEqual(stdout, [
      'sub-dep v1',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
      `Restarting ${inspect(file)}`,
      'sub-dep v1',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
    ]);
  });

  it('should watch changes to worker - esm', async () => {
    const worker = path.join(dir, 'worker.mjs');

    writeFileSync(worker, `
console.log('worker running');
`);

    const file = createTmpFile(`
import { Worker } from 'node:worker_threads';
new Worker(new URL(${JSON.stringify(pathToFileURL(worker))}));
`, '.mjs', dir);

    const { stderr, stdout } = await runWriteSucceed({
      file,
      watchedFile: worker,
    });

    assert.strictEqual(stderr, '');
    assert.deepStrictEqual(stdout, [
      'worker running',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
      `Restarting ${inspect(file)}`,
      'worker running',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
    ]);
  });

  it('should watch changes to worker dependencies - esm', async () => {
    const dep = path.join(dir, 'dep.mjs');
    const worker = path.join(dir, 'worker.mjs');

    writeFileSync(dep, `
export default 'dep v1';
`);

    writeFileSync(worker, `
import dep from ${JSON.stringify(pathToFileURL(dep))};
console.log(dep);
`);

    const file = createTmpFile(`
import { Worker } from 'node:worker_threads';
new Worker(new URL(${JSON.stringify(pathToFileURL(worker))}));
`, '.mjs', dir);

    const { stderr, stdout } = await runWriteSucceed({
      file,
      watchedFile: dep,
    });

    assert.strictEqual(stderr, '');
    assert.deepStrictEqual(stdout, [
      'dep v1',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
      `Restarting ${inspect(file)}`,
      'dep v1',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
    ]);
  });

  it('should watch changes to nested worker dependencies - esm', async () => {
    const subDep = path.join(dir, 'sub-dep.mjs');
    const dep = path.join(dir, 'dep.mjs');
    const worker = path.join(dir, 'worker.mjs');

    writeFileSync(subDep, `
export default 'sub-dep v1';
`);

    writeFileSync(dep, `
import subDep from ${JSON.stringify(pathToFileURL(subDep))};
console.log(subDep);
export default 'dep v1';
`);

    writeFileSync(worker, `
import dep from ${JSON.stringify(pathToFileURL(dep))};
`);

    const file = createTmpFile(`
import { Worker } from 'node:worker_threads';
new Worker(new URL(${JSON.stringify(pathToFileURL(worker))}));
`, '.mjs', dir);

    const { stderr, stdout } = await runWriteSucceed({
      file,
      watchedFile: subDep,
    });

    assert.strictEqual(stderr, '');
    assert.deepStrictEqual(stdout, [
      'sub-dep v1',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
      `Restarting ${inspect(file)}`,
      'sub-dep v1',
      `Completed running ${inspect(file)}. Waiting for file changes before restarting...`,
    ]);
  });
});
