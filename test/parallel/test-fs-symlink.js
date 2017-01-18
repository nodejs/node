'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

let linkTime;
let fileTime;

if (!common.canCreateSymLink()) {
  common.skip('insufficient privileges');
  return;
}

common.refreshTmpDir();

// test creating and reading symbolic link
const linkData = path.join(common.fixturesDir, '/cycles/root.js');
const linkPath = path.join(common.tmpDir, 'symlink1.js');

fs.symlink(linkData, linkPath, common.mustCall(function(err) {
  assert.ifError(err);

  fs.lstat(linkPath, common.mustCall(function(err, stats) {
    assert.ifError(err);
    linkTime = stats.mtime.getTime();
  }));

  fs.stat(linkPath, common.mustCall(function(err, stats) {
    assert.ifError(err);
    fileTime = stats.mtime.getTime();
  }));

  fs.readlink(linkPath, common.mustCall(function(err, destination) {
    assert.ifError(err);
    assert.strictEqual(destination, linkData);
  }));
}));


process.on('exit', function() {
  assert.notStrictEqual(linkTime, fileTime);
});
