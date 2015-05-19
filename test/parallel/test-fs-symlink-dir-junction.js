'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var completed = 0;
var expected_tests = 4;

// test creating and reading symbolic link
var linkData = path.join(common.fixturesDir, 'cycles/');
var linkPath = path.join(common.tmpDir, 'cycles_link');

// Delete previously created link
try {
  fs.unlinkSync(linkPath);
} catch (e) {}

console.log('linkData: ' + linkData);
console.log('linkPath: ' + linkPath);

fs.symlink(linkData, linkPath, 'junction', function(err) {
  if (err) throw err;
  completed++;

  fs.lstat(linkPath, function(err, stats) {
    if (err) throw err;
    assert.ok(stats.isSymbolicLink());
    completed++;

    fs.readlink(linkPath, function(err, destination) {
      if (err) throw err;
      assert.equal(destination, linkData);
      completed++;

      fs.unlink(linkPath, function(err) {
        if (err) throw err;
        assert(!common.fileExists(linkPath));
        assert(common.fileExists(linkData));
        completed++;
      });
    });
  });
});

process.on('exit', function() {
  assert.equal(completed, expected_tests);
});

