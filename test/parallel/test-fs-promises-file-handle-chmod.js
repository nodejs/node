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

async function validateFilePermission({ filePath, fileHandle }) {
  //file created with r/w, should not have execute
  fs.access(filePath, fs.constants.X_OK, common.mustCall(async (err) => {
    //err should an instance of Error
    assert.deepStrictEqual(err instanceof Error, true);
    //add execute permissions
    await fileHandle.chmod(0o765);

    fs.access(filePath, fs.constants.X_OK, common.mustCall((err) => {
      //err should be undefined or null
      assert.deepStrictEqual(!err, true);
    }));
  }));
}

async function executeTests() {
  tmpdir.refresh();
  common.crashOnUnhandledRejection();

  const filePath = path.resolve(tmpDir, 'tmp-chmod.txt');
  const chmodFileDetails = {
    filePath,
    fileHandle: await open(filePath, 'w+', 0o666)
  };

  await validateFilePermission(chmodFileDetails);
}

executeTests().then(common.mustCall());
