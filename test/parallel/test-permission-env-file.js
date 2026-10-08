'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');

tmpdir.refresh();

const envFile = tmpdir.resolve('permission.env');
fs.writeFileSync(
  envFile,
  'PERMISSION_ENV_FROM_FILE=file\nPERMISSION_ENV_BOTH=file\n');

const env = {
  ...process.env,
  PERMISSION_ENV_BOTH: 'inherited',
};

const script = `console.log(JSON.stringify({
  fromFile: process.env.PERMISSION_ENV_FROM_FILE,
  both: process.env.PERMISSION_ENV_BOTH,
  has: [
    process.permission.has('env', 'PERMISSION_ENV_FROM_FILE'),
    process.permission.has('env', 'PERMISSION_ENV_BOTH'),
  ],
}))`;

// Variables defined in an env file are allowed. Only the file's values are
// visible: an inherited value for the same name was removed at startup.
spawnSyncAndAssert(
  process.execPath,
  ['--permission', `--env-file=${envFile}`, '-e', script],
  { env },
  {
    stdout(output) {
      assert.deepStrictEqual(JSON.parse(output), {
        fromFile: 'file',
        both: 'file',
        has: [true, true],
      });
    },
  });

// A name that --allow-env grants access to keeps the usual precedence of the
// inherited value over the file's.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission',
    '--allow-env=PERMISSION_ENV_BOTH',
    `--env-file=${envFile}`,
    '-e', script,
  ],
  { env },
  {
    stdout(output) {
      assert.deepStrictEqual(JSON.parse(output), {
        fromFile: 'file',
        both: 'inherited',
        has: [true, true],
      });
    },
  });
