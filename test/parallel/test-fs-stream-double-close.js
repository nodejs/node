'use strict';
var common = require('../common');
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
  stream.on('open', common.mustCall(function(fd) {
    stream.destroy();
  }));
}

function test3(stream) {
  stream.on('open', common.mustCall(function(fd) {
    stream.destroy();
    stream.destroy();
  }));
}
