'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const example = path.join(common.fixturesDir, 'x.txt');

const regex = /^'options' argument must be a string or object/;

assert.doesNotThrow(() => {
  fs.createReadStream(example, undefined);
});
assert.doesNotThrow(() => {
  fs.createReadStream(example, 'utf8');
});
assert.doesNotThrow(() => {
  fs.createReadStream(example, {encoding: 'utf8'});
});

assert.throws(() => {
  fs.createReadStream(example, null);
}, err => regex.test(err.message));
assert.throws(() => {
  fs.createReadStream(example, 123);
}, err => regex.test(err.message));
assert.throws(() => {
  fs.createReadStream(example, 0);
}, err => regex.test(err.message));
assert.throws(() => {
  fs.createReadStream(example, true);
}, err => regex.test(err.message));
assert.throws(() => {
  fs.createReadStream(example, false);
}, err => regex.test(err.message));
