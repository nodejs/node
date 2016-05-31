'use strict';
var common = require('../common');
var fs = require('fs');
var path = require('path');
var assert = require('assert');
var os = require('os');

if (!common.isWindows) {
  common.skip('this test is Windows-specific.');
  return;
}

var successes = 0;

var tmpDir = common.tmpDir;
var useOsTmpDir = false;

// use real OS tmp dir on Linux when it is readable/writable
// to avoid failing tests on ecryptfs filesystems:
// https://github.com/nodejs/node/issues/2255
if (os.type() == 'Linux') {
  try {
    fs.accessSync(os.tmpdir(), fs.R_OK | fs.W_OK);
    useOsTmpDir = true;
    tmpDir = path.join(os.tmpdir(),
      'node-' + process.version + '-test-' + (Math.random() * 1e6 | 0));
    fs.mkdirSync(tmpDir);
  } catch (e) {
    useOsTmpDir = false;
    tmpDir = common.tmpDir;
  }
}

// make a path that will be at least 260 chars long.
var fileNameLen = Math.max(260 - tmpDir.length - 1, 1);
var fileName = path.join(tmpDir, new Array(fileNameLen + 1).join('x'));
var fullPath = path.resolve(fileName);

if (!useOsTmpDir) {
  common.refreshTmpDir();
}

console.log({
  filenameLength: fileName.length,
  fullPathLength: fullPath.length
});

fs.writeFile(fullPath, 'ok', function(err) {
  if (err) throw err;
  successes++;

  fs.stat(fullPath, function(err, stats) {
    if (err) throw err;
    successes++;
  });
});

process.on('exit', function() {
  fs.unlinkSync(fullPath);
  if (useOsTmpDir) {
    fs.rmdirSync(tmpDir);
  }
  assert.equal(2, successes);
});
