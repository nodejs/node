'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

// This test ensures that fs.rmSync can handle UTF-8 characters
// in file paths without errors.

tmpdir.refresh(); // Prepare a clean temporary directory

const filePath = path.join(tmpdir.path, '速.txt'); // Use tmpdir.path for the file location

// Create a file with a name containing non-ASCII characters
fs.writeFileSync(filePath, 'This is a test file with special characters.');

// fs.rmSync should not throw an error for non-ASCII file names
fs.rmSync(filePath);

// Ensure the file has been removed
assert.strictEqual(fs.existsSync(filePath), false);
