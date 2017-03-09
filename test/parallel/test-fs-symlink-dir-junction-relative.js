'use strict';
// Test creating and resolving relative junction or symbolic link

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const linkPath1 = path.join(common.tmpDir, 'junction1');
const linkPath2 = path.join(common.tmpDir, 'junction2');
const linkTarget = path.join(common.fixturesDir);
const linkData = path.join(common.fixturesDir);

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
  const stats = fs.lstatSync(linkPath);
  assert.ok(stats.isSymbolicLink());

  const data1 = fs.readFileSync(linkPath + '/x.txt', 'ascii');
  const data2 = fs.readFileSync(linkTarget + '/x.txt', 'ascii');
  assert.strictEqual(data1, data2);

  // Clean up.
  fs.unlinkSync(linkPath);
}
