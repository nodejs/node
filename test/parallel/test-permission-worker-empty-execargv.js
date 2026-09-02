'use strict';
const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) common.skip('This test only works on a main thread');
if (!common.hasCrypto) common.skip('no crypto');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const allowedDir = tmpdir.path;
const allowedFile = path.join(allowedDir, 'ok.txt');
const subDir = path.join(allowedDir, 'sub');
const subFile = path.join(subDir, 'nested.txt');
fs.mkdirSync(subDir, { recursive: true });

const deniedDir = fs.mkdtempSync(
  path.join(path.dirname(tmpdir.path), 'perm-deny-'),
);
const deniedFile = path.join(deniedDir, 'secret.txt');

fs.writeFileSync(allowedFile, 'allowed\n');
fs.writeFileSync(subFile, 'nested\n');
fs.writeFileSync(deniedFile, 'secret\n');

const allowedDirSlash = allowedDir.endsWith(path.sep)
  ? allowedDir
  : allowedDir + path.sep;

function runWithParent(parentArgs, workerBody, execArgvFragment) {
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
    w.on('error', (err) => { console.error(err); process.exit(1); });
  `;
  return spawnSync(
    process.execPath,
    parentArgs.concat(['-e', code]),
    { encoding: 'utf8', timeout: 30000, env: { ...process.env } },
  );
}

function baseParent(extra = []) {
  return [
    '--permission',
    `--allow-fs-read=${allowedDir}`,
    '--allow-worker',
    ...extra,
  ];
}

function parseLastJsonLine(stdout) {
  const lines = stdout.trim().split('\n').filter(Boolean);
  assert.ok(lines.length > 0, `expected JSON, got: ${stdout}`);
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

function workerWrite(filePath) {
  return `
    const { parentPort } = require('worker_threads');
    const fs = require('fs');
    try {
      fs.writeFileSync(${JSON.stringify(filePath)}, 'w');
      parentPort.postMessage({ ok: true });
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
  // default Worker: parent allowlist applies
  {
    const r = runWithParent(baseParent(), workerRead(deniedFile), '');
    assert.strictEqual(r.status, 0, r.stderr);
    assertAccessDenied(parseLastJsonLine(r.stdout));
  }

  // execArgv: [] ceiling
  {
    const denied = runWithParent(
      baseParent(), workerRead(deniedFile), 'execArgv: [],');
    assert.strictEqual(denied.status, 0, denied.stderr);
    assertAccessDenied(parseLastJsonLine(denied.stdout));

    const allowed = runWithParent(
      baseParent(), workerRead(allowedFile), 'execArgv: [],');
    assert.strictEqual(allowed.status, 0, allowed.stderr);
    assertOk(parseLastJsonLine(allowed.stdout));

    const nested = runWithParent(
      baseParent(), workerRead(subFile), 'execArgv: [],');
    assert.strictEqual(nested.status, 0, nested.stderr);
    assertOk(parseLastJsonLine(nested.stdout));
  }

  // non-permission flag only
  {
    const denied = runWithParent(
      baseParent(), workerRead(deniedFile), 'execArgv: ["--no-warnings"],');
    assert.strictEqual(denied.status, 0, denied.stderr);
    assertAccessDenied(parseLastJsonLine(denied.stdout));
    const allowed = runWithParent(
      baseParent(), workerRead(allowedFile), 'execArgv: ["--no-warnings"],');
    assert.strictEqual(allowed.status, 0, allowed.stderr);
    assertOk(parseLastJsonLine(allowed.stdout));
  }

  // permission on, no fs-read → empty grants
  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission', '--allow-worker',
    ])},`;
    const a = runWithParent(baseParent(), workerRead(allowedFile), frag);
    assert.strictEqual(a.status, 0, a.stderr);
    assertAccessDenied(parseLastJsonLine(a.stdout));
  }

  // escalate * clamped
  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission', '--allow-fs-read=*', '--allow-worker',
    ])},`;
    const denied = runWithParent(baseParent(), workerRead(deniedFile), frag);
    assert.strictEqual(denied.status, 0, denied.stderr);
    assertAccessDenied(parseLastJsonLine(denied.stdout));
    const allowed = runWithParent(baseParent(), workerRead(allowedFile), frag);
    assert.strictEqual(allowed.status, 0, allowed.stderr);
    assertOk(parseLastJsonLine(allowed.stdout));
  }

  // "*" plus a concrete path still yields the full parent list
  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission', '--allow-fs-read=*', `--allow-fs-read=${allowedFile}`,
      '--allow-worker',
    ])},`;
    const denied = runWithParent(baseParent(), workerRead(deniedFile), frag);
    assert.strictEqual(denied.status, 0, denied.stderr);
    assertAccessDenied(parseLastJsonLine(denied.stdout));
    const allowed = runWithParent(baseParent(), workerRead(allowedFile), frag);
    assert.strictEqual(allowed.status, 0, allowed.stderr);
    assertOk(parseLastJsonLine(allowed.stdout));
  }

  // repeated flags cannot exceed parent
  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      `--allow-fs-read=${deniedFile}`,
      '--allow-worker',
    ])},`;
    const r = runWithParent(baseParent(), workerRead(deniedFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertAccessDenied(parseLastJsonLine(r.stdout));
  }

  // subset path under parent
  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission', `--allow-fs-read=${subFile}`, '--allow-worker',
    ])},`;
    const r = runWithParent(baseParent(), workerRead(subFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }

  // trailing-slash parent allow
  {
    const parent = [
      '--permission', `--allow-fs-read=${allowedDirSlash}`, '--allow-worker',
    ];
    const r = runWithParent(parent, workerRead(allowedFile), 'execArgv: [],');
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }

  // parent * → worker path kept
  {
    const parent = [
      '--permission', '--allow-fs-read=*', '--allow-worker',
    ];
    const frag = `execArgv: ${JSON.stringify([
      '--permission', `--allow-fs-read=${deniedFile}`, '--allow-worker',
    ])},`;
    const r = runWithParent(parent, workerRead(deniedFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }

  // permission-audit + empty execArgv
  {
    const parent = [
      '--permission', '--permission-audit',
      `--allow-fs-read=${allowedDir}`, '--allow-worker',
    ];
    const r = runWithParent(parent, workerRead(allowedFile), 'execArgv: [],');
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }

  // fs-write: parent lacks write → denied; parent grants → ok
  {
    const writeTarget = path.join(allowedDir, 'w.txt');
    const frag = `execArgv: ${JSON.stringify([
      '--permission', `--allow-fs-write=${allowedDir}`, '--allow-worker',
    ])},`;
    const blocked = runWithParent(baseParent(), workerWrite(writeTarget), frag);
    assert.strictEqual(blocked.status, 0, blocked.stderr);
    assertAccessDenied(parseLastJsonLine(blocked.stdout));

    const parent = [
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      `--allow-fs-write=${allowedDir}`,
      '--allow-worker',
    ];
    const ok = runWithParent(parent, workerWrite(writeTarget), frag);
    assert.strictEqual(ok.status, 0, ok.stderr);
    assertOk(parseLastJsonLine(ok.stdout));
  }

  // empty execArgv inherits parent write
  {
    const writeTarget = path.join(allowedDir, 'w2.txt');
    const parent = [
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      `--allow-fs-write=${allowedDir}`,
      '--allow-worker',
    ];
    const r = runWithParent(parent, workerWrite(writeTarget), 'execArgv: [],');
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }

  // boolean AND: worker asks allow-net, parent does not (still fs-clamped)
  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      '--allow-net',
      '--allow-worker',
    ])},`;
    const r = runWithParent(baseParent(), workerRead(deniedFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertAccessDenied(parseLastJsonLine(r.stdout));
  }

  // rebuild: several booleans when parent grants them
  {
    const parent = [
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      '--allow-worker',
      '--allow-addons',
      '--allow-inspector',
      '--allow-net',
    ];
    const frag = `execArgv: ${JSON.stringify([
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      '--allow-worker',
      '--allow-addons',
      '--allow-inspector',
      '--allow-net',
    ])},`;
    const r = runWithParent(parent, workerRead(allowedFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }

  // worker permission-audit only → empty fs list restrict
  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission', '--permission-audit', '--allow-worker',
    ])},`;
    const a = runWithParent(baseParent(), workerRead(allowedFile), frag);
    assert.strictEqual(a.status, 0, a.stderr);
    assertAccessDenied(parseLastJsonLine(a.stdout));
  }

  // --no-warnings kept through rebuild with permission flags
  {
    const frag = `execArgv: ${JSON.stringify([
      '--no-warnings',
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      '--allow-worker',
    ])},`;
    const r = runWithParent(baseParent(), workerRead(allowedFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }

  // exact file grant + shorter path rejected
  {
    const parent = [
      '--permission', `--allow-fs-read=${allowedFile}`, '--allow-worker',
    ];
    const frag = `execArgv: ${JSON.stringify([
      '--permission', `--allow-fs-read=${allowedFile}`, '--allow-worker',
    ])},`;
    const ok = runWithParent(parent, workerRead(allowedFile), frag);
    assert.strictEqual(ok.status, 0, ok.stderr);
    assertOk(parseLastJsonLine(ok.stdout));
    const bad = runWithParent(parent, workerRead(allowedDir), frag);
    assert.strictEqual(bad.status, 0, bad.stderr);
    assertAccessDenied(parseLastJsonLine(bad.stdout));
  }

  // sibling path not under parent
  {
    const sibling = path.join(path.dirname(allowedDir), 'other-sibling-x');
    const frag = `execArgv: ${JSON.stringify([
      '--permission', `--allow-fs-read=${sibling}`, '--allow-worker',
    ])},`;
    const r = runWithParent(baseParent(), workerRead(deniedFile), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertAccessDenied(parseLastJsonLine(r.stdout));
  }

  // space-form --allow-fs-read <path>
  {
    const frag = `execArgv: ${JSON.stringify([
      '--permission', '--allow-fs-read', allowedDir, '--allow-worker',
    ])},`;
    const ok = runWithParent(baseParent(), workerRead(allowedFile), frag);
    assert.strictEqual(ok.status, 0, ok.stderr);
    assertOk(parseLastJsonLine(ok.stdout));
    const denied = runWithParent(baseParent(), workerRead(deniedFile), frag);
    assert.strictEqual(denied.status, 0, denied.stderr);
    assertAccessDenied(parseLastJsonLine(denied.stdout));
  }

  // space-form --allow-fs-write <path>
  {
    const writeTarget = path.join(allowedDir, 'w3.txt');
    const parent = [
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      `--allow-fs-write=${allowedDir}`,
      '--allow-worker',
    ];
    const frag = `execArgv: ${JSON.stringify([
      '--permission', '--allow-fs-write', allowedDir, '--allow-worker',
    ])},`;
    const r = runWithParent(parent, workerWrite(writeTarget), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }

  // write * → fall back to parent write list
  {
    const writeTarget = path.join(allowedDir, 'w4.txt');
    const parent = [
      '--permission',
      `--allow-fs-read=${allowedDir}`,
      `--allow-fs-write=${allowedDir}`,
      '--allow-worker',
    ];
    const frag = `execArgv: ${JSON.stringify([
      '--permission', '--allow-fs-write=*', '--allow-worker',
    ])},`;
    const r = runWithParent(parent, workerWrite(writeTarget), frag);
    assert.strictEqual(r.status, 0, r.stderr);
    assertOk(parseLastJsonLine(r.stdout));
  }
} finally {
  fs.rmSync(deniedDir, { recursive: true, force: true });
}
