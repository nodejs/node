'use strict';

// Test that readdir callback works with Buffer paths in recursive mode.
// Refs: https://github.com/nodejs/node/issues/58892

const common = require('../common');

const { readdir } = require('node:fs');
const { join } = require('node:path');

const testDirPath = join(__dirname, '..', '..');
readdir(Buffer.from(testDirPath), { recursive: true }, common.mustCall());
