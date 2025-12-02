'use strict';

// Test that readdirSync works with Buffer paths in recursive mode.
// Refs: https://github.com/nodejs/node/issues/58892

require('../common');

const { readdirSync } = require('node:fs');
const { join } = require('node:path');

const testDirPath = join(__dirname, '..', '..');
readdirSync(Buffer.from(testDirPath), { recursive: true });
