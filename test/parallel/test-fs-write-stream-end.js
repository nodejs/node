'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

(function() {
  var file = path.join(common.tmpDir, 'write-end-test0.txt');
  var stream = fs.createWriteStream(file);
  stream.end();
  stream.on('close', common.mustCall(function() { }));
})();

(function() {
  var file = path.join(common.tmpDir, 'write-end-test1.txt');
  var stream = fs.createWriteStream(file);
  stream.end('a\n', 'utf8');
  stream.on('close', common.mustCall(function() {
    var content = fs.readFileSync(file, 'utf8');
    assert.equal(content, 'a\n');
  }));
})();
