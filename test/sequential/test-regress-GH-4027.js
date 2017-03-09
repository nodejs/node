'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

common.refreshTmpDir();

const filename = path.join(common.tmpDir, 'watched');
fs.writeFileSync(filename, 'quis custodiet ipsos custodes');
setTimeout(fs.unlinkSync, 100, filename);

fs.watchFile(filename, { interval: 50 }, common.mustCall(function(curr, prev) {
  assert.equal(prev.nlink, 1);
  assert.equal(curr.nlink, 0);
  fs.unwatchFile(filename);
}));
