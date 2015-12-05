'use strict';
const common = require('../common');
const fs = require('fs');
const path = require('path');
const os = require('os');

// when it fails test again under real OS tmp dir on Linux when it is
// readable/writable to avoid failing tests on ecryptfs filesystems:
// https://github.com/nodejs/node/issues/2255
// it follows advice in comments to:
// https://github.com/nodejs/node/pull/3925
// https://github.com/nodejs/node/pull/3929
try {
  common.refreshTmpDir();
  testRequireLongPath(common.tmpDir);
  common.refreshTmpDir();
} catch (e) {
  if (os.type() == 'Linux') {
    fs.accessSync(os.tmpdir(), fs.R_OK | fs.W_OK);
    const tmpDir = path.join(os.tmpdir(),
      `node-${process.version}-test-${1e6 * Math.random() | 0}`);
    fs.mkdirSync(tmpDir);
    testRequireLongPath(tmpDir);
    fs.rmdirSync(tmpDir);
  } else {
    throw e;
  }
}

function testRequireLongPath(tmpDir) {

  // make a path that is more than 260 chars long.
  const dirNameLen = Math.max(260 - tmpDir.length, 1);
  const dirName = path.join(tmpDir, 'x'.repeat(dirNameLen));
  const fullDirPath = path.resolve(dirName);

  const indexFile = path.join(fullDirPath, 'index.js');
  const otherFile = path.join(fullDirPath, 'other.js');

  fs.mkdirSync(fullDirPath);
  fs.writeFileSync(indexFile, 'require("./other");');
  fs.writeFileSync(otherFile, '');

  require(indexFile);
  require(otherFile);

  fs.unlinkSync(indexFile);
  fs.unlinkSync(otherFile);
  fs.rmdirSync(fullDirPath);

}
