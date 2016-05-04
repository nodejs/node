'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const example = path.join(common.tmpDir, 'dummy');

common.refreshTmpDir();

assert.doesNotThrow(function() {
  fs.createWriteStream(example, undefined);
});
assert.doesNotThrow(function() {
  fs.createWriteStream(example, 'utf8');
});
assert.doesNotThrow(function() {
  fs.createWriteStream(example, {encoding: 'utf8'});
});

common.throws(function() {
  fs.createWriteStream(example, null);
}, {code: 'INVALIDARG'});
common.throws(function() {
  fs.createWriteStream(example, 123);
}, {code: 'INVALIDARG'});
common.throws(function() {
  fs.createWriteStream(example, 0);
}, {code: 'INVALIDARG'});
common.throws(function() {
  fs.createWriteStream(example, true);
}, {code: 'INVALIDARG'});
common.throws(function() {
  fs.createWriteStream(example, false);
}, {code: 'INVALIDARG'});
