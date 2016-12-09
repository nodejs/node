'use strict';
// Test creating and resolving relative junction or symbolic link

var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var linkPath1 = path.join(common.tmpDir, 'junction1');
var linkPath2 = path.join(common.tmpDir, 'junction2');
var linkTarget = path.join(common.fixturesDir);
var linkData = path.join(common.fixturesDir);

common.refreshTmpDir();

// Test fs.symlink()
fs.symlink(linkData, linkPath1, 'junction', common.mustCall(function(err) {
  if (err) throw err;
  verifyLink(linkPath1);
}));

// Test fs.symlinkSync()
fs.symlinkSync(linkData, linkPath2, 'junction');
verifyLink(linkPath2);

function verifyLink(linkPath) {
  var stats = fs.lstatSync(linkPath);
  assert.ok(stats.isSymbolicLink());

  var data1 = fs.readFileSync(linkPath + '/x.txt', 'ascii');
  var data2 = fs.readFileSync(linkTarget + '/x.txt', 'ascii');
  assert.strictEqual(data1, data2);

  // Clean up.
  fs.unlinkSync(linkPath);
}
