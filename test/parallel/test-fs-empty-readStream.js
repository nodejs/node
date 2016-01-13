'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var emptyFile = path.join(common.fixturesDir, 'empty.txt');

fs.open(emptyFile, 'r', function(error, fd) {
  assert.ifError(error);

  var read = fs.createReadStream(emptyFile, { 'fd': fd });

  read.once('data', function() {
    throw new Error('data event should not emit');
  });

  read.once('end', common.mustCall(function endEvent1() {}));
});

fs.open(emptyFile, 'r', function(error, fd) {
  assert.ifError(error);

  var read = fs.createReadStream(emptyFile, { 'fd': fd });
  read.pause();

  read.once('data', function() {
    throw new Error('data event should not emit');
  });

  read.once('end', function endEvent2() {
    throw new Error('end event should not emit');
  });

  setTimeout(function() {
    assert.equal(read.isPaused(), true);
  }, common.platformTimeout(50));
});
