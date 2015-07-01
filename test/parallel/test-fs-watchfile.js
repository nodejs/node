'use strict';

const fs = require('fs');
const path = require('path');
const assert = require('assert');
const common = require('../common');
const fixtures = path.join(__dirname, '..', 'fixtures');

// Basic usage tests.
assert.throws(function() {
  fs.watchFile('./some-file');
}, /watchFile requires a listener function/);

assert.throws(function() {
  fs.watchFile('./another-file', {}, 'bad listener');
}, /watchFile requires a listener function/);

assert.throws(function() {
  fs.watchFile(new Object(), function() {});
}, /Path must be a string/);

// Test ENOENT. Should fire once.
const enoentFile = path.join(fixtures, 'empty', 'non-existent-file');
fs.watchFile(enoentFile, common.mustCall(function(curr, prev) {
  fs.unwatchFile(enoentFile);
}));
