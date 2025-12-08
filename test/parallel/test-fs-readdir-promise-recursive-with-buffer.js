'use strict';

// Test that fs.promises.readdir works with Buffer paths in recursive mode.
// Refs: https://github.com/nodejs/node/issues/58892

const common = require('../common');

const { readdir } = require('node:fs/promises');
const { join } = require('node:path');

const testDirPath = join(__dirname, '..', '..');
readdir(Buffer.from(testDirPath), { recursive: true }).then(common.mustCall());

readdir(Buffer.from(testDirPath), { recursive: true, withFileTypes: true }).then(common.mustCall());
