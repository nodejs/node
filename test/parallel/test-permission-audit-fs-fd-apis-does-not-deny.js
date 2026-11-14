'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const target = tmpdir.resolve('audit-fd-apis.txt');
fs.copyFileSync(fixtures.path('permission', 'deny', 'regular-file.md'), target);

function run(flag, op) {
  const childScript = `
    const fs = require('node:fs');
    (async () => {
      const fd = fs.openSync(process.env.TARGET, 'r+');
      const fh = await fs.promises.open(process.env.TARGET, 'r+');
      const cb = (fn, ...args) => new Promise((resolve, reject) => {
        fn(fd, ...args, (err) => err ? reject(err) : resolve());
      });
      try {
        await (${op});
        console.log('RESULT NO_THROW');
      } catch (e) {
        console.log('RESULT THREW ' + e.code);
      } finally {
        await fh.close();
        fs.closeSync(fd);
      }
    })();
  `;
  const { status, stdout, stderr } = spawnSync(
    process.execPath,
    [
      flag,
      '--allow-fs-read=*',
      '--allow-fs-write=*',
      '--allow-env=TARGET',
      '-e',
      childScript,
    ],
    { encoding: 'utf8', env: { ...process.env, TARGET: target } },
  );
  assert.strictEqual(status, 0, stderr);
  const resultLine = stdout.split('\n').find((l) => l.startsWith('RESULT '));
  assert.ok(resultLine, stdout);
  return resultLine.replace('RESULT ', '');
}

const ops = [
  ['fs.fsyncSync()', 'fs.fsyncSync(fd)'],
  ['fs.fsync()', 'cb(fs.fsync)'],
  ['filehandle.sync()', 'fh.sync()'],
  ['fs.fdatasyncSync()', 'fs.fdatasyncSync(fd)'],
  ['fs.fdatasync()', 'cb(fs.fdatasync)'],
  ['filehandle.datasync()', 'fh.datasync()'],
  ['fs.fchmodSync()', 'fs.fchmodSync(fd, 0o644)'],
  ['fs.fchmod()', 'cb(fs.fchmod, 0o644)'],
  ['filehandle.chmod()', 'fh.chmod(0o644)'],
  ['fs.fchownSync()', 'fs.fchownSync(fd, -1, -1)'],
  ['fs.fchown()', 'cb(fs.fchown, -1, -1)'],
  ['filehandle.chown()', 'fh.chown(-1, -1)'],
  ['fs.futimesSync()', 'fs.futimesSync(fd, 1, 1)'],
  ['fs.futimes()', 'cb(fs.futimes, 1, 1)'],
  ['filehandle.utimes()', 'fh.utimes(1, 1)'],
];

for (const [api, op] of ops) {
  test(`audit mode does not disable ${api}`, () => {
    assert.strictEqual(run('--permission-audit', op), 'NO_THROW');
  });

  test(`enforce mode still disables ${api}`, () => {
    assert.strictEqual(run('--permission', op), 'THREW ERR_ACCESS_DENIED');
  });
}
