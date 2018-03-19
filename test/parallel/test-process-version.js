'use strict';
require('../common');

const assert = require('assert');

const {
  majorVersion: major,
  minorVersion: minor,
  patchVersion: patch,
} = process.release;

assert.strictEqual(
  (major << 16) + (minor << 8) + patch, process.computedVersion);
