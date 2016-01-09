'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const example = path.join(common.tmpDir, 'dummy');

const regex = /^'options' argument must be a string or object/;

common.refreshTmpDir();

assert.doesNotThrow(() => {
  fs.createWriteStream(example, undefined);
});
assert.doesNotThrow(() => {
  fs.createWriteStream(example, 'utf8');
});
assert.doesNotThrow(() => {
  fs.createWriteStream(example, {encoding: 'utf8'});
});

assert.throws(() => {
  fs.createWriteStream(example, null);
}, err => regex.test(err.message));
assert.throws(() => {
  fs.createWriteStream(example, 123);
}, err => regex.test(err.message));
assert.throws(() => {
  fs.createWriteStream(example, 0);
}, err => regex.test(err.message));
assert.throws(() => {
  fs.createWriteStream(example, true);
}, err => regex.test(err.message));
assert.throws(() => {
  fs.createWriteStream(example, false);
}, err => regex.test(err.message));
