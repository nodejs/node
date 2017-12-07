'use strict';
const common = require('../common');

// Test that unlink succeeds immediately after readFile completes.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const fileName = path.resolve(common.tmpDir, 'test.bin');

const buf = Buffer.alloc(512 * 1024, 42);

common.refreshTmpDir();

fs.writeFileSync(fileName, buf);

fs.readFile(fileName, function(err, data) {
  assert.ifError(err);
  assert.strictEqual(data.length, buf.length);
  assert.strictEqual(buf[0], 42);

  // Unlink should not throw. This is part of the test. It used to throw on
  // Windows due to a bug.
  fs.unlinkSync(fileName);
});
