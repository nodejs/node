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
