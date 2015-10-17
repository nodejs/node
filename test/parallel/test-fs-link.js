'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const exec = require('child_process').exec;
var completed = 0;
var expected_async;
var linkTime;
var fileTime;

common.refreshTmpDir();

const runTest = function() {
  if (!skip_symlinks) {
    // test creating and reading symbolic link
    const linkData = path.join(common.fixturesDir, '/cycles/root.js');
    const linkPath = path.join(common.tmpDir, 'symlink1.js');

    fs.symlink(linkData, linkPath, function(err) {
      if (err) throw err;
      console.log('symlink done');

      fs.lstat(linkPath, function(err, stats) {
        if (err) throw err;
        linkTime = stats.mtime.getTime();
        completed++;
      });

      fs.stat(linkPath, function(err, stats) {
        if (err) throw err;
        fileTime = stats.mtime.getTime();
        completed++;
      });

      fs.readlink(linkPath, function(err, destination) {
        if (err) throw err;
        assert.equal(destination, linkData);
        completed++;
      });
    });
  }

  // test creating and reading hard link
  const srcPath = path.join(common.fixturesDir, 'cycles', 'root.js');
  const dstPath = path.join(common.tmpDir, 'link1.js');

  fs.link(srcPath, dstPath, function(err) {
    if (err) throw err;
    console.log('hard link done');
    const srcContent = fs.readFileSync(srcPath, 'utf8');
    const dstContent = fs.readFileSync(dstPath, 'utf8');
    assert.equal(srcContent, dstContent);
    completed++;
  });
};

var skip_symlinks = false;
var expected_async = 4;
if (common.isWindows) {
  // On Windows, creating symlinks requires admin privileges.
  // We'll only try to run symlink test if we have enough privileges.
  exec('whoami /priv', function(err, o) {
    if (err || o.indexOf('SeCreateSymbolicLinkPrivilege') == -1) {
      expected_async = 1;
      skip_symlinks = true;
    }
  });
}
runTest();

process.on('exit', function() {
  assert.equal(completed, expected_async);
  if (! skip_symlinks) {
    assert.notStrictEqual(linkTime, fileTime);
  }
});
