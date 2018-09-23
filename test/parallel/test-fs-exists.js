'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var f = __filename;
var exists;
var doesNotExist;

fs.exists(f, function(y) {
  exists = y;
});

fs.exists(f + '-NO', function(y) {
  doesNotExist = y;
});

assert(fs.existsSync(f));
assert(!fs.existsSync(f + '-NO'));

process.on('exit', function() {
  assert.strictEqual(exists, true);
  assert.strictEqual(doesNotExist, false);
});
