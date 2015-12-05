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
  testFsLongPath(common.tmpDir);
  common.refreshTmpDir();
} catch (e) {
  if (os.type() == 'Linux') {
    fs.accessSync(os.tmpdir(), fs.R_OK | fs.W_OK);
    const tmpDir = path.join(os.tmpdir(),
      `node-${process.version}-test-${1e6 * Math.random() | 0}`);
    fs.mkdirSync(tmpDir);
    testFsLongPath(tmpDir);
    fs.rmdirSync(tmpDir);
  } else {
    throw e;
  }
}

function testFsLongPath(tmpDir) {

  // make a path that will be at least 260 chars long.
  const fileNameLen = Math.max(260 - tmpDir.length - 1, 1);
  const fileName = path.join(tmpDir, new Array(fileNameLen + 1).join('x'));
  const fullPath = path.resolve(fileName);

  console.log({
    filenameLength: fileName.length,
    fullPathLength: fullPath.length
  });

  fs.writeFileSync(fullPath, 'ok');
  fs.statSync(fullPath);
  fs.unlinkSync(fullPath);

}
