'use strict';
var common = require('../common');
var assert = require('assert');

var fs = require('fs');
var path = require('path');

var file = path.join(common.fixturesDir, 'x.txt');
var data = '';
var first = true;

var stream = fs.createReadStream(file);
stream.setEncoding('utf8');
stream.on('data', function(chunk) {
  data += chunk;
  if (first) {
    first = false;
    stream.resume();
  }
});

process.nextTick(function() {
  stream.pause();
  setTimeout(function() {
    stream.resume();
  }, 100);
});

process.on('exit', function() {
  assert.equal(data, 'xyz\n');
});
