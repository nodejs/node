'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var tmp = common.tmpDir;
common.refreshTmpDir();
var filename = path.resolve(tmp, 'truncate-file.txt');

var success = 0;

fs.writeFileSync(filename, 'hello world', 'utf8');
var fd = fs.openSync(filename, 'r+');
fs.truncate(fd, 5, function(err) {
  assert.ok(!err);
  assert.equal(fs.readFileSync(filename, 'utf8'), 'hello');
  success++;
});

process.on('exit', function() {
  fs.closeSync(fd);
  fs.unlinkSync(filename);
  assert.equal(success, 1);
  console.log('ok');
});
