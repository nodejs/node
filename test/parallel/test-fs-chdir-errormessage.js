'use strict';

require('../common');
const process = require('process');
const assert = require('assert');

assert.throws(
  () => {
    process.chdir('does-not-exist');
  },
  (error) => {
    assert.strictEqual(error instanceof Error, true);
    assert.strictEqual(
      error.message,
      "ENOENT: no such file or directory, chdir 'does-not-exist'"
    );
    return true;
  }
);
