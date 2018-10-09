'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const filename = __filename.toLowerCase();

assert.strictEqual(
  fs.realpathSync.native('./test/parallel/test-fs-realpath-native.js')
    .toLowerCase(),
  filename);

fs.realpath.native(
  './test/parallel/test-fs-realpath-native.js',
  common.mustCall(function(err, res) {
    assert.ifError(err);
    assert.strictEqual(res.toLowerCase(), filename);
    assert.strictEqual(this, undefined);
  }));
