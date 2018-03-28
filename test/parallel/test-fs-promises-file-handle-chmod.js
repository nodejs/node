'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs/promises
// FileHandle.chmod method.

const fs = require('fs');
const { open } = require('fs/promises');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

async function validateFilePermission() {
  tmpdir.refresh();
  common.crashOnUnhandledRejection();

  const filePath = path.resolve(tmpDir, 'tmp-chmod.txt');
  const fileHandle = await open(filePath, 'w+', 0o644);
  //file created with r/w 644
  const statsBeforeMod = fs.statSync(filePath);
  assert.deepStrictEqual(statsBeforeMod.mode & 0o644, 0o644);
  //change the permissions to 765
  await fileHandle.chmod(0o765);
  const statsAfterMod = fs.statSync(filePath);
  assert.deepStrictEqual(statsAfterMod.mode & 0o765, 0o765);
}

validateFilePermission().then(common.mustCall());
