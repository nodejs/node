'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const example = path.join(common.fixturesDir, 'x.txt');

assert.doesNotThrow(function() {
  fs.createReadStream(example, undefined);
});
assert.doesNotThrow(function() {
  fs.createReadStream(example, 'utf8');
});
assert.doesNotThrow(function() {
  fs.createReadStream(example, {encoding: 'utf8'});
});

common.throws(function() {
  fs.createReadStream(example, null);
}, {code: 'INVALIDARG'});
common.throws(function() {
  fs.createReadStream(example, 123);
}, {code: 'INVALIDARG'});
common.throws(function() {
  fs.createReadStream(example, 0);
}, {code: 'INVALIDARG'});
common.throws(function() {
  fs.createReadStream(example, true);
}, {code: 'INVALIDARG'});
common.throws(function() {
  fs.createReadStream(example, false);
}, {code: 'INVALIDARG'});
