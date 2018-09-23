'use strict';
var common = require('../common');
var assert = require('assert');

var zlib = require('zlib');
var gz = zlib.Gzip();
var emptyBuffer = new Buffer(0);
var received = 0;
gz.on('data', function(c) {
  received += c.length;
});
var ended = false;
gz.on('end', function() {
  ended = true;
});
var finished = false;
gz.on('finish', function() {
  finished = true;
});
gz.write(emptyBuffer);
gz.end();

process.on('exit', function() {
  assert.equal(received, 20);
  assert(ended);
  assert(finished);
  console.log('ok');
});
