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
const allowedFile = path.join(allowed, 'ok.txt');
const deniedFile = path.join(tmpdir.path, '..', 'permission-worker-denied-file');
fs.writeFileSync(allowedFile, 'allowed\n');
fs.writeFileSync(deniedFile, 'secret\n');

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

function lastMsg(r) {
  assert.strictEqual(r.status, 0, r.stderr);
  return JSON.parse(r.stdout.trim().split('\n').pop());
}

function srcRead(file) {
  return `
    const { parentPort } = require('worker_threads');
    const fs = require('fs');
    try {
      parentPort.postMessage({
        ok: true,
        data: fs.readFileSync(${JSON.stringify(file)}, 'utf8'),
      });
    } catch (err) {
      parentPort.postMessage({ ok: false, code: err.code });
    }
  `;
}

// default: denied blocked
{
  const msg = lastMsg(runWorker(srcRead(deniedFile), ''));
  assert.strictEqual(msg.ok, false);
  assert.strictEqual(msg.code, 'ERR_ACCESS_DENIED');
}

// execArgv []: denied blocked, allowed readable
{
  const denied = lastMsg(runWorker(srcRead(deniedFile), 'execArgv: [],'));
  assert.strictEqual(denied.ok, false, JSON.stringify(denied));
  assert.strictEqual(denied.code, 'ERR_ACCESS_DENIED');
  const ok = lastMsg(runWorker(srcRead(allowedFile), 'execArgv: [],'));
  assert.strictEqual(ok.ok, true, JSON.stringify(ok));
}

// non-permission flag only: same boundary
{
  const denied = lastMsg(runWorker(srcRead(deniedFile), 'execArgv: ["--no-warnings"],'));
  assert.strictEqual(denied.ok, false);
  assert.strictEqual(denied.code, 'ERR_ACCESS_DENIED');
  const ok = lastMsg(runWorker(srcRead(allowedFile), 'execArgv: ["--no-warnings"],'));
  assert.strictEqual(ok.ok, true, JSON.stringify(ok));
}

// wider than parent
{
  const frag = `execArgv: ${JSON.stringify([
    '--permission', '--allow-fs-read=*', '--allow-worker',
  ])},`;
  const msg = lastMsg(runWorker(srcRead(deniedFile), frag));
  assert.strictEqual(msg.ok, false, JSON.stringify(msg));
  assert.strictEqual(msg.code, 'ERR_ACCESS_DENIED');
}

// repeated allow flags in worker execArgv still cannot exceed parent
{
  const frag = `execArgv: ${JSON.stringify([
    '--permission',
    `--allow-fs-read=${allowed}`,
    `--allow-fs-read=${deniedFile}`,
    '--allow-worker',
  ])},`;
  const msg = lastMsg(runWorker(srcRead(deniedFile), frag));
  assert.strictEqual(msg.ok, false, JSON.stringify(msg));
  assert.strictEqual(msg.code, 'ERR_ACCESS_DENIED');
}
