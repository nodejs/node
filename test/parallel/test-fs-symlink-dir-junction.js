'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

// test creating and reading symbolic link
var linkData = path.join(common.fixturesDir, 'cycles/');
var linkPath = path.join(common.tmpDir, 'cycles_link');

common.refreshTmpDir();

console.log('linkData: ' + linkData);
console.log('linkPath: ' + linkPath);

fs.symlink(linkData, linkPath, 'junction', common.mustCall(function(err) {
  if (err) throw err;

  fs.lstat(linkPath, common.mustCall(function(err, stats) {
    if (err) throw err;
    assert.ok(stats.isSymbolicLink());

    fs.readlink(linkPath, common.mustCall(function(err, destination) {
      if (err) throw err;
      assert.equal(destination, linkData);

      fs.unlink(linkPath, common.mustCall(function(err) {
        if (err) throw err;
        assert(!common.fileExists(linkPath));
        assert(common.fileExists(linkData));
      }));
    }));
  }));
}));
