'use strict';

const common = require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

if (!common.hasCrypto) {
  common.skip('no crypto');
}

// We need to define the flags dynamically to account for the `NODE_TEST_DIR` env var.
if (!process.permission) {
  spawnSyncAndExitWithoutError(process.execPath, [
    '--permission',
    '--allow-fs-read=*', `--allow-fs-write=${process.env.NODE_TEST_DIR || './test'}/.tmp.*`, '--allow-child-process',
    __filename,
  ]);
  return;
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

{
  assert.throws(() => {
    process.report.writeReport('./secret.txt');
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: './secret.txt',
  }));
}

{
  assert.throws(() => {
    process.report.writeReport();
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: process.cwd(),
  }));
}

{
  const reportPath = path.join(tmpdir.path, 'report.json');
  spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--permission',
      '--allow-fs-read=*',
      `--allow-fs-write=${tmpdir.path}/*`,
      '-e',
      `process.report.writeReport(${JSON.stringify(reportPath)})`,
    ]
  );
}

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--permission',
    '--allow-fs-read=*',
    `--allow-fs-write=${tmpdir.path}`,
    '-e',
    'process.report.writeReport()',
  ],
  { cwd: tmpdir.path }
);

{
  const allowedDir = path.join(tmpdir.path, 'report-allowed');
  const deniedDir = path.join(tmpdir.path, 'report-denied');
  fs.mkdirSync(allowedDir);
  fs.mkdirSync(deniedDir);

  const deniedFile = path.join(deniedDir, 'report.json');
  fs.writeFileSync(deniedFile, 'existing content');
  spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--permission',
      '--allow-fs-read=*',
      `--allow-fs-write=${allowedDir}`,
      '-e',
      `
      const assert = require('assert');
      process.report.directory = ${JSON.stringify(deniedDir)};
      assert.throws(() => {
        process.report.writeReport('report.json');
      }, {
        code: 'ERR_ACCESS_DENIED',
        permission: 'FileSystemWrite',
        resource: ${JSON.stringify(deniedFile)},
      });
      `,
    ],
    { cwd: allowedDir },
  );
  assert.strictEqual(fs.readFileSync(deniedFile, 'utf8'), 'existing content');
}

{
  const allowedDir = path.join(tmpdir.path, 'report-filename-allowed');
  const deniedDir = path.join(tmpdir.path, 'report-filename-denied');
  fs.mkdirSync(allowedDir);
  fs.mkdirSync(deniedDir);

  const deniedFile = path.join(deniedDir, 'report.json');
  spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--permission',
      '--allow-fs-read=*',
      `--allow-fs-write=${allowedDir}`,
      '-e',
      `
      const assert = require('assert');
      process.report.directory = ${JSON.stringify(deniedDir)};
      process.report.filename = 'report.json';
      assert.throws(() => {
        process.report.writeReport();
      }, {
        code: 'ERR_ACCESS_DENIED',
        permission: 'FileSystemWrite',
        resource: ${JSON.stringify(deniedFile)},
      });
      `,
    ],
    { cwd: allowedDir },
  );
  assert.strictEqual(fs.existsSync(deniedFile), false);
}
