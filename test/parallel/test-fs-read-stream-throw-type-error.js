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

assert.throws(function() {
  fs.createReadStream(example, null);
}, /"options" argument must be a string or an object/);
assert.throws(function() {
  fs.createReadStream(example, 123);
}, /"options" argument must be a string or an object/);
assert.throws(function() {
  fs.createReadStream(example, 0);
}, /"options" argument must be a string or an object/);
assert.throws(function() {
  fs.createReadStream(example, true);
}, /"options" argument must be a string or an object/);
assert.throws(function() {
  fs.createReadStream(example, false);
}, /"options" argument must be a string or an object/);
