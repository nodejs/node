'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

assert.throws(function() {
  fs.watch('non-existent-file');
}, function(err) {
  assert(err);
  assert(/non-existent-file/.test(err));
  assert.equal(err.filename, 'non-existent-file');
  return true;
});

const watcher = fs.watch(__filename);
watcher.on('error', common.mustCall(function(err) {
  assert(err);
  assert(/non-existent-file/.test(err));
  assert.equal(err.filename, 'non-existent-file');
}));
watcher._handle.onchange(-1, 'ENOENT', 'non-existent-file');
