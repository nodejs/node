'use strict';
const assert = require('node:assert');
const { unlinkSync, writeFileSync } = require('node:fs')
const { join } = require('node:path');
const { test } = require('node:test');
const common = require('./common');

test('third 1', () => {
  common.fnC(1, 4);
  common.fnD(99);
});

assert(process.env.NODE_TEST_TMPDIR);
const tmpFilePath = join(process.env.NODE_TEST_TMPDIR, 'temp-module.js');
writeFileSync(tmpFilePath, `
  module.exports = {
    fn() {
      return 42;
    }
  };
`);
const tempModule = require(tmpFilePath);
assert.strictEqual(tempModule.fn(), 42);
// Deleted files should not be part of the coverage report.
unlinkSync(tmpFilePath);
