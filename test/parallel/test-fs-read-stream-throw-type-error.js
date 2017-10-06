'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');

const example = fixtures.path('x.txt');

assert.doesNotThrow(function() {
  fs.createReadStream(example, undefined);
});
assert.doesNotThrow(function() {
  fs.createReadStream(example, null);
});
assert.doesNotThrow(function() {
  fs.createReadStream(example, 'utf8');
});
assert.doesNotThrow(function() {
  fs.createReadStream(example, { encoding: 'utf8' });
});

const errMessage = /"options" must be a string or an object/;
assert.throws(function() {
  fs.createReadStream(example, 123);
}, errMessage);
assert.throws(function() {
  fs.createReadStream(example, 0);
}, errMessage);
assert.throws(function() {
  fs.createReadStream(example, true);
}, errMessage);
assert.throws(function() {
  fs.createReadStream(example, false);
}, errMessage);
