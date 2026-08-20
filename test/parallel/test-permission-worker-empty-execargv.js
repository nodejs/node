'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

if (!common.hasCrypto) {
  common.skip('no crypto');
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const allowedDir = tmpdir.path;
const allowedFile = path.join(allowedDir, 'ok.txt');
const subFile = path.join(allowedDir, 'sub', 'nested.txt');
fs.mkdirSync(path.dirname(subFile), { recursive: true });

const deniedDir = fs.mkdtempSync(
  path.join(path.dirname(tmpdir.path), 'perm-deny-'),
);
const deniedFile = path.join(deniedDir, 'secret.txt');

fs.writeFileSync(allowedFile, 'allowed\n');
fs.writeFileSync(subFile, 'nested\n');
fs.writeFileSync(deniedFile, 'secret\n');

function runWorkerCase(workerBody, execArgvFragment) {
  const code = `
    const { Worker } = require('worker_threads');
    const w = new Worker(${JSON.stringify(workerBody)}, {
      eval: true,
      ${execArgvFragment}
    });
    w.on('message', (msg) => {
      process.stdout.write(JSON.stringify(msg) + '\\n');
      process.exit(0);
    });
    w.on('error', (err) => {
      console.error(err);
      process.exit(1);
    });
  `;
  return spawnSync(
    process.execPath,
    [
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      '--allow-worker',
      '-e',
      code,
    ],
    {
      encoding: 'utf8',
      timeout: 30000,
      env: { ...process.env },
    },
  );
}

function parseLastJsonLine(stdout) {
  const lines = stdout.trim().split('\n').filter(Boolean);
  assert.ok(lines.length > 0, 'expected worker JSON output');
  return JSON.parse(lines[lines.length - 1]);
}

function workerRead(filePath) {
  return `
    const { parentPort } = require('worker_threads');
    const fs = require('fs');
    try {
      parentPort.postMessage({
        ok: true,
        data: fs.readFileSync(${JSON.stringify(filePath)}, 'utf8'),
      });
    } catch (err) {
      parentPort.postMessage({ ok: false, code: err.code });
    }
  `;
}

function assertAccessDenied(msg) {
  assert.strictEqual(msg.ok, false, JSON.stringify(msg));
  assert.strictEqual(msg.code, 'ERR_ACCESS_DENIED');
}

function assertOk(msg) {
  assert.strictEqual(msg.ok, true, JSON.stringify(msg));
}

try {
  {
    const r = runWorkerCase(workerRead(deniedFile), '');
    assert.strictEqual(r.status, 0, r.stderr);
    assertAccessDenied(parseLastJsonLine(r.stdout));
  }

  {
    const denied = runWorkerCase(workerRead(deniedFile), 'execArgv: [],');
    assert.strictEqual(denied.status, 0, denied.stderr);
    assertAccessDenied(parseLastJsonLine(denied.stdout));

    const allowed = runWorkerCase(workerRead(allowedFile), 'execArgv: [],');
    assert.strictEqual(allowed.status, 0, allowed.stderr);
    assertOk(parseLastJsonLine(allowed.stdout));

    const nested = runWorkerCase(workerRead(subFile), 'execArgv: [],');
    assert.strictEqual(nested.status, 0, nested.stderr);
    assertOk(parseLastJsonLine(nested.stdout));
  }

  {
    const denied = runWorkerCase(
      workerRead(deniedFile),
      'execArgv: ["--no-warnings"],',
    );
    assert.strictEqual(denied.status, 0, denied.stderr);
    assertAccessDenied(parseLastJsonLine(denied.stdout));

    const allowed = runWorkerCase(
      workerRead(allowedFile),
      'execArgv: ["--no-warnings"],',
    );
    assert.strictEqual(allowed.status, 0, allowed.stderr);
    assertOk(parseLastJsonLine(allowed.stdout));
  }

  // --permission --allow-worker with no allow-fs-read → no fs grants (empty list)
  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission',
      '--allow-worker',
    ])},`;
    const denied = runWorkerCase(workerRead(deniedFile), frag);
    assert.strictEqual(denied.status, 0, denied.stderr);
    assertAccessDenied(parseLastJsonLine(denied.stdout));

    const allowed = runWorkerCase(workerRead(allowedFile), frag);
    assert.strictEqual(allowed.status, 0, allowed.stderr);
    assertAccessDenied(parseLastJsonLine(allowed.stdout));
  }

  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission',
      '--allow-fs-read=*',
      '--allow-worker',
    ])},`;
    const r = runWorkerCase(workerRead(deniedFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertAccessDenied(parseLastJsonLine(r.stdout));
  }

  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      `--allow-fs-read=${deniedFile}`,
      '--allow-worker',
    ])},`;
    const r = runWorkerCase(workerRead(deniedFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertAccessDenied(parseLastJsonLine(r.stdout));
  }

  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission',
      `--allow-fs-read=${subFile}`,
      '--allow-worker',
    ])},`;
    const r = runWorkerCase(workerRead(subFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }
} finally {
  fs.rmSync(deniedDir, { recursive: true, force: true });
}
