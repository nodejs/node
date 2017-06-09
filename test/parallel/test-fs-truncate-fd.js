'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const tmp = common.tmpDir;
common.refreshTmpDir();
const filename = path.resolve(tmp, 'truncate-file.txt');

fs.writeFileSync(filename, 'hello world', 'utf8');
const fd = fs.openSync(filename, 'r+');
fs.truncate(fd, 5, common.mustCall(function(err) {
  assert.ok(!err);
  assert.strictEqual(fs.readFileSync(filename, 'utf8'), 'hello');
}));

process.on('exit', function() {
  fs.closeSync(fd);
  fs.unlinkSync(filename);
  console.log('ok');
});
