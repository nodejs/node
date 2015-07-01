'use strict';

const fs = require('fs');
const assert = require('assert');

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
