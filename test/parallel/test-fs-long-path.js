'use strict';
var common = require('../common');
var fs = require('fs');
var path = require('path');
var assert = require('assert');
var os = require('os');

// when it fails test again under real OS tmp dir on Linux when it is
// readable/writable to avoid failing tests on ecryptfs filesystems:
// https://github.com/nodejs/node/issues/2255
// it follows advice in comments to:
// https://github.com/nodejs/node/pull/3925
try {
  common.refreshTmpDir();
  testFsLongPath(common.tmpDir);
  common.refreshTmpDir();
} catch (e) {
  if (os.type() == 'Linux') {
    fs.accessSync(os.tmpdir(), fs.R_OK | fs.W_OK);
    var tmpDir = path.join(os.tmpdir(),
      'node-' + process.version + '-test-' + (Math.random() * 1e6 | 0));
    fs.mkdirSync(tmpDir);
    testFsLongPath(tmpDir);
    fs.rmdirSync(tmpDir);
  } else {
    throw e;
  }
}

function testFsLongPath(tmpDir) {

  // make a path that will be at least 260 chars long.
  var fileNameLen = Math.max(260 - tmpDir.length - 1, 1);
  var fileName = path.join(tmpDir, new Array(fileNameLen + 1).join('x'));
  var fullPath = path.resolve(fileName);

  console.log({
    filenameLength: fileName.length,
    fullPathLength: fullPath.length
  });

  fs.writeFileSync(fullPath, 'ok');
  fs.statSync(fullPath);
  fs.unlinkSync(fullPath);

}
