'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
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
