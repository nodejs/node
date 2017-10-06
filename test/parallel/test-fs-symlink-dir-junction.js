'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

// test creating and reading symbolic link
const linkData = fixtures.path('cycles/');
const linkPath = path.join(common.tmpDir, 'cycles_link');

common.refreshTmpDir();

fs.symlink(linkData, linkPath, 'junction', common.mustCall(function(err) {
  if (err) throw err;

  fs.lstat(linkPath, common.mustCall(function(err, stats) {
    if (err) throw err;
    assert.ok(stats.isSymbolicLink());

    fs.readlink(linkPath, common.mustCall(function(err, destination) {
      if (err) throw err;
      assert.strictEqual(destination, linkData);

      fs.unlink(linkPath, common.mustCall(function(err) {
        if (err) throw err;
        assert(!common.fileExists(linkPath));
        assert(common.fileExists(linkData));
      }));
    }));
  }));
}));
