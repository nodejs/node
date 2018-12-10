'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.chmod method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

async function validateFilePermission() {
  const filePath = path.resolve(tmpDir, 'tmp-chmod.txt');
  const fileHandle = await open(filePath, 'w+', 0o444);
  // file created with r--r--r-- 444
  const statsBeforeMod = fs.statSync(filePath);
  assert.deepStrictEqual(statsBeforeMod.mode & 0o444, 0o444);

  let expectedAccess;
  const newPermissions = 0o765;

  if (common.isWindows) {
    // Chmod in Windows will only toggle read only/write access. The
    // fs.Stats.mode in Windows is computed using read/write
    // bits (not exec). Read-only at best returns 444; r/w 666.
    // Refer: /deps/uv/src/win/fs.cfs;
    expectedAccess = 0o664;
  } else {
    expectedAccess = newPermissions;
  }

  // change the permissions to rwxr--r-x
  await fileHandle.chmod(newPermissions);
  const statsAfterMod = fs.statSync(filePath);
  assert.deepStrictEqual(statsAfterMod.mode & expectedAccess, expectedAccess);
}

validateFilePermission().then(common.mustCall());
