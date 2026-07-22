'use strict';

// This test will fail because the the implementation does not properly
// handle the case when the path is a Buffer and the function is called
// in recursive mode.

// Refs: https://github.com/nodejs/node/issues/58892

const common = require('../common');

const { readdir } = require('node:fs/promises');
const { join } = require('node:path');

const testDirPath = join(__dirname, '..', '..');
readdir(Buffer.from(testDirPath), { recursive: true }).then(common.mustCall());
