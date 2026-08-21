'use strict';

// This test will fail because the the implementation does not properly
// handle the case when the path is a Buffer and the function is called
// in recursive mode.

// Refs: https://github.com/nodejs/node/issues/58892

require('../common');

const { readdirSync } = require('node:fs');
const { join } = require('node:path');

const testDirPath = join(__dirname, '..', '..');
readdirSync(Buffer.from(testDirPath), { recursive: true });
