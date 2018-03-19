'use strict';
require('../common');

const assert = require('assert');

// eslint-disable-next-line prefer-const
let [major, minor, patch] = process.version.split('.');
major = major.slice(1);
patch = patch.slice('-')[0];

assert.strictEqual(
  (major << 16) + (minor << 8) + +patch, process.computedVersion);
