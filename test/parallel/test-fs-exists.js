'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const f = __filename;

fs.exists(f, common.mustCall(function(y) {
  assert.strictEqual(y, true);
}));

fs.exists(f + '-NO', common.mustCall(function(y) {
  assert.strictEqual(y, false);
}));

assert(fs.existsSync(f));
assert(!fs.existsSync(f + '-NO'));
