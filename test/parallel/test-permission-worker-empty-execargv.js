'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');
if (!isMainThread) common.skip('main thread only');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const allowed = tmpdir.path;
const deniedFile = path.join(tmpdir.path, '..', 'permission-worker-denied-file');
fs.writeFileSync(deniedFile, 'secret\n');
fs.writeFileSync(path.join(allowed, 'ok.txt'), 'allowed\n');

function runWorker(workerSource, execArgvFragment) {
  const code = `
    const { Worker } = require('worker_threads');
    const w = new Worker(${JSON.stringify(workerSource)}, {
      eval: true,
      ${execArgvFragment}
    });
    w.on('message', (msg) => {
      process.stdout.write(JSON.stringify(msg) + '\\n');
      process.exit(0);
    });
    w.on('error', (err) => { console.error(err); process.exit(1); });
  `;
  return spawnSync(process.execPath, [
    '--permission',
    `--allow-fs-read=${allowed}`,
    '--allow-worker',
    '-e',
    code,
  ], { encoding: 'utf8', timeout: 20000 });
}

const readDenied = `
  const { parentPort } = require('worker_threads');
  const fs = require('fs');
  try {
    parentPort.postMessage({
      ok: true,
      data: fs.readFileSync(${JSON.stringify(deniedFile)}, 'utf8'),
    });
  } catch (err) {
    parentPort.postMessage({ ok: false, code: err.code });
  }
`;

const readAllowed = `
  const { parentPort } = require('worker_threads');
  const fs = require('fs');
  try {
    parentPort.postMessage({
      ok: true,
      data: fs.readFileSync(${JSON.stringify(path.join(allowed, 'ok.txt'))}, 'utf8'),
    });
  } catch (err) {
    parentPort.postMessage({ ok: false, code: err.code });
  }
`;

function lastMsg(r) {
  assert.strictEqual(r.status, 0, r.stderr);
  return JSON.parse(r.stdout.trim().split('\n').pop());
}

{
  const msg = lastMsg(runWorker(readDenied, ''));
  assert.strictEqual(msg.ok, false);
  assert.strictEqual(msg.code, 'ERR_ACCESS_DENIED');
}

{
  const msg = lastMsg(runWorker(readDenied, 'execArgv: [],'));
  assert.strictEqual(msg.ok, false, JSON.stringify(msg));
  assert.strictEqual(msg.code, 'ERR_ACCESS_DENIED');
}

{
  const msg = lastMsg(runWorker(readAllowed, 'execArgv: [],'));
  assert.strictEqual(msg.ok, true, JSON.stringify(msg));
}

{
  const frag = `execArgv: ${JSON.stringify([
    '--permission',
    '--allow-fs-read=*',
    '--allow-worker',
  ])},`;
  const msg = lastMsg(runWorker(readDenied, frag));
  assert.strictEqual(msg.ok, false, JSON.stringify(msg));
  assert.strictEqual(msg.code, 'ERR_ACCESS_DENIED');
}
