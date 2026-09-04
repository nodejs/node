'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

// This test ensures that --permission-audit does not deny fs.lstat() and
// fs.symlink(), whose permission checks live in the JavaScript layer, while
// --permission still denies them. Audit mode must publish the denial through
// the diagnostics channel and let the operation continue. Each API is covered
// in its three flavours (sync, callback and promise), since every flavour
// carries its own copy of the check.

const assert = require('assert');
const { spawnSync } = require('child_process');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

const blockedFile = fixtures.path('permission', 'deny', 'protected-file.md');

function run(flag, { op, name }) {
  const childScript = `
    const dc = require('node:diagnostics_channel');
    const msgs = [];
    dc.subscribe('node:permission-model:fs', (m) => msgs.push({
      permission: m.permission,
      resource: m.resource,
    }));
    (async () => {
      try {
        await (${op});
        console.log('RESULT NO_THROW');
      } catch (e) {
        console.log('RESULT THREW ' + e.code);
      }
      console.log('AUDIT ' + JSON.stringify(msgs));
    })();
  `;

  const env = {
    ...process.env,
    BLOCKED_FILE: blockedFile,
    LINK_PATH: tmpdir.resolve(`audit-symlink-${flag.replace(/\W/g, '')}-${name}`),
  };
  const { status, stdout, stderr } = spawnSync(
    process.execPath,
    [flag, '-e', childScript],
    { encoding: 'utf8', env },
  );
  assert.strictEqual(status, 0, stderr);
  const lines = stdout.split('\n');
  const resultLine = lines.find((l) => l.startsWith('RESULT '));
  assert.ok(resultLine, stdout);
  const auditLine = lines.find((l) => l.startsWith('AUDIT '));
  assert.ok(auditLine, stdout);
  return {
    result: resultLine.replace('RESULT ', ''),
    msgs: JSON.parse(auditLine.replace('AUDIT ', '')),
  };
}

const lstatOps = [
  {
    name: 'lstatSync',
    api: 'fs.lstatSync()',
    op: 'require("node:fs").lstatSync(process.env.BLOCKED_FILE)',
  },
  {
    name: 'lstatCallback',
    api: 'fs.lstat()',
    op: 'new Promise((resolve, reject) => require("node:fs").lstat(' +
      'process.env.BLOCKED_FILE, (err) => err ? reject(err) : resolve()))',
  },
  {
    name: 'lstatPromises',
    api: 'fsPromises.lstat()',
    op: 'require("node:fs").promises.lstat(process.env.BLOCKED_FILE)',
  },
];

const symlinkOps = [
  {
    name: 'symlinkSync',
    api: 'fs.symlinkSync()',
    op: 'require("node:fs").symlinkSync(process.env.BLOCKED_FILE, ' +
      'process.env.LINK_PATH)',
  },
  {
    name: 'symlinkCallback',
    api: 'fs.symlink()',
    op: 'new Promise((resolve, reject) => require("node:fs").symlink(' +
      'process.env.BLOCKED_FILE, process.env.LINK_PATH, ' +
      '(err) => err ? reject(err) : resolve()))',
  },
  {
    name: 'symlinkPromises',
    api: 'fsPromises.symlink()',
    op: 'require("node:fs").promises.symlink(process.env.BLOCKED_FILE, ' +
      'process.env.LINK_PATH)',
  },
];

tmpdir.refresh();

for (const entry of lstatOps) {
  test(`audit mode does not deny ${entry.api} but logs the denial`, () => {
    const { result, msgs } = run('--permission-audit', entry);
    assert.strictEqual(result, 'NO_THROW');
    assert.strictEqual(msgs.length, 1);
    assert.strictEqual(msgs[0].permission, 'FileSystemRead');
    assert.ok(msgs[0].resource.endsWith('protected-file.md'));
  });

  test(`enforce mode still denies ${entry.api}`, () => {
    const { result } = run('--permission', entry);
    assert.strictEqual(result, 'THREW ERR_ACCESS_DENIED');
  });
}

for (const entry of symlinkOps) {
  test(`audit mode does not deny ${entry.api} but logs the denial`, (t) => {
    if (!common.canCreateSymLink()) {
      return t.skip('insufficient privileges to create symlinks');
    }
    const { result, msgs } = run('--permission-audit', entry);
    assert.strictEqual(result, 'NO_THROW');
    assert.ok(
      msgs.some((m) => m.permission === 'FileSystem'),
      JSON.stringify(msgs),
    );
  });

  test(`enforce mode still denies ${entry.api}`, (t) => {
    if (!common.canCreateSymLink()) {
      return t.skip('insufficient privileges to create symlinks');
    }
    const { result } = run('--permission', entry);
    assert.strictEqual(result, 'THREW ERR_ACCESS_DENIED');
  });
}
