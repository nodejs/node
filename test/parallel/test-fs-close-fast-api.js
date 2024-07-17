'use strict';

require('../common');

const assert = require('assert');
const fs = require('fs');

// This test runs `fs.readFile` which calls ReadFileContext
// that triggers binding.close() when read operation is done.
// The goal of this test is to not crash
let val;

// For loop is required to trigger fast API.
for (let i = 0; i < 100_000; i++) {
  try {
    val = fs.readFile(__filename, (a, b) => {
      try {
        assert.strictEqual(a, null);
        assert.ok(b.length > 0);
      } catch {
        // Ignore all errors
      }
    });
  } catch {
    // do nothing
  }
}

console.log(val);
