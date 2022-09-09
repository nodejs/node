'use strict';
require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');

if (process.env.isChild === '1') {
  setImmediate(() => {
    if (process.env.mode === 'eval') {
      eval('require("fs").statSync(__filename);');
    } else {
      require('fs').statSync(__filename);
    }
  });
  return;
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  const FILE_NAME = path.join(tmpdir.path, 'sync_io_file_eval.log');
  cp.spawnSync(process.execPath,
               [
                 '--trace-sync-io',
                 '--trace-sync-io-file-name=sync_io_file_eval.log',
                 __filename,
               ],
               {
                 cwd: tmpdir.path,
                 env: {
                   ...process.env,
                   isChild: '1',
                   mode: 'eval'
                 }
               });

  console.log(fs.readFileSync(FILE_NAME, 'utf-8'));
  assert(fs.existsSync(FILE_NAME));
}

{
  const FILE_NAME = path.join(tmpdir.path, 'sync_io_file.log');
  cp.spawnSync(process.execPath,
               [
                 '--trace-sync-io',
                 '--trace-sync-io-file-name=sync_io_file.log',
                 __filename,
               ],
               {
                 cwd: tmpdir.path,
                 env: {
                   ...process.env,
                   isChild: '1',
                 }
               });

  console.log(fs.readFileSync(FILE_NAME, 'utf-8'));
  assert(fs.existsSync(FILE_NAME));
}

{
  const FILE_NAME = path.join(tmpdir.path, 'sync_io_file_env_variable.log');
  cp.spawnSync(process.execPath,
               [
                 '--trace-sync-io',
                 __filename,
               ],
               {
                 cwd: tmpdir.path,
                 env: {
                   ...process.env,
                   isChild: '1',
                   NODE_OPTIONS: '--trace-sync-io-file-name=sync_io_file_env_variable.log'
                 }
               });

  console.log(fs.readFileSync(FILE_NAME, 'utf-8'));
  assert(fs.existsSync(FILE_NAME));
}
