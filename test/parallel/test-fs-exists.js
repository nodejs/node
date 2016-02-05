'use strict';
require('../common');
var assert = require('assert');
var fs = require('fs');
var f = __filename;
var exists;
var doesNotExist;
var exists2;
var doesNotExist2;

fs.exists(f, function(y) {
  exists = y;
});

fs.exists(f + '-NO', function(y) {
  doesNotExist = y;
});

fs.exists(f, function(err, y) {
  exists2 = y;
});

fs.exists(f + '-NO', function(err, y) {
  doesNotExist2 = y;
});

assert(fs.existsSync(f));
assert(!fs.existsSync(f + '-NO'));

process.on('exit', function() {
  assert.strictEqual(exists, true);
  assert.strictEqual(doesNotExist, false);
  assert.strictEqual(exists2, true);
  assert.strictEqual(doesNotExist2, false);
});
