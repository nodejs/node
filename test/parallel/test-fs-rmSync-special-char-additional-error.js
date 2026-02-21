'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

tmpdir.refresh();

const file = path.join(tmpdir.path, '速_file');
fs.writeFileSync(file, 'x');

// Treat that file as if it were a directory so the error message
// includes the path treated as a directory, not the file.
const badPath = path.join(file, 'child');

// Attempt to delete the directory which should now fail
try {
  fs.rmSync(badPath, { recursive: true });
} catch (err) {
  // Verify that the error is due to the path being treated as a directory
  assert.strictEqual(err.code, 'ENOTDIR');
  assert.strictEqual(err.path, badPath);
  assert(err.message.includes(badPath), 'Error message should include the path treated as a directory');
}
