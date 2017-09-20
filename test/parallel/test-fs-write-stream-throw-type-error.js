'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const numberError =
  /^TypeError: "options" must be a string or an object, got number instead\.$/;

const booleanError =
  /^TypeError: "options" must be a string or an object, got boolean instead\.$/;

const example = path.join(common.tmpDir, 'dummy');

common.refreshTmpDir();

assert.doesNotThrow(() => {
  fs.createWriteStream(example, undefined);
});

assert.doesNotThrow(() => {
  fs.createWriteStream(example, null);
});

assert.doesNotThrow(() => {
  fs.createWriteStream(example, 'utf8');
});

assert.doesNotThrow(() => {
  fs.createWriteStream(example, { encoding: 'utf8' });
});

assert.throws(() => {
  fs.createWriteStream(example, 123);
}, numberError);

assert.throws(() => {
  fs.createWriteStream(example, 0);
}, numberError);

assert.throws(() => {
  fs.createWriteStream(example, true);
}, booleanError);

assert.throws(() => {
  fs.createWriteStream(example, false);
}, booleanError);
