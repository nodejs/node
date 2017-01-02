'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

common.refreshTmpDir();

var filename = path.join(common.tmpDir, 'watched');
fs.writeFileSync(filename, 'quis custodiet ipsos custodes');
setTimeout(fs.unlinkSync, 100, filename);

fs.watchFile(filename, { interval: 50 }, common.mustCall(function(curr, prev) {
  assert.equal(prev.nlink, 1);
  assert.equal(curr.nlink, 0);
  fs.unwatchFile(filename);
}));
