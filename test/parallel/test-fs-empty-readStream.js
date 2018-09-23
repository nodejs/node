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

  var readEmit = false;
  read.once('end', function() {
    readEmit = true;
    console.error('end event 1');
  });

  setTimeout(function() {
    assert.equal(readEmit, true);
  }, common.platformTimeout(50));
});

fs.open(emptyFile, 'r', function(error, fd) {
  assert.ifError(error);

  var read = fs.createReadStream(emptyFile, { 'fd': fd });
  read.pause();

  read.once('data', function() {
    throw new Error('data event should not emit');
  });

  var readEmit = false;
  read.once('end', function() {
    readEmit = true;
    console.error('end event 2');
  });

  setTimeout(function() {
    assert.equal(readEmit, false);
  }, common.platformTimeout(50));
});
