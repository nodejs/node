'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

common.refreshTmpDir();

test1(fs.createReadStream(__filename));
test2(fs.createReadStream(__filename));
test3(fs.createReadStream(__filename));

test1(fs.createWriteStream(common.tmpDir + '/dummy1'));
test2(fs.createWriteStream(common.tmpDir + '/dummy2'));
test3(fs.createWriteStream(common.tmpDir + '/dummy3'));

function test1(stream) {
  stream.destroy();
  stream.destroy();
}

function test2(stream) {
  stream.destroy();
  stream.on('open', function(fd) {
    stream.destroy();
    open_cb_called++;
  });
  process.on('exit', function() {
    assert.equal(open_cb_called, 1);
  });
  var open_cb_called = 0;
}

function test3(stream) {
  stream.on('open', function(fd) {
    stream.destroy();
    stream.destroy();
    open_cb_called++;
  });
  process.on('exit', function() {
    assert.equal(open_cb_called, 1);
  });
  var open_cb_called = 0;
}
