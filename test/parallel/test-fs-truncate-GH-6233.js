'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const filename = common.tmpDir + '/truncate-file.txt';

common.refreshTmpDir();

// Synchronous test.
{
  fs.writeFileSync(filename, '0123456789');
  assert.strictEqual(fs.readFileSync(filename).toString(), '0123456789');
  fs.truncateSync(filename, 5);
  assert.strictEqual(fs.readFileSync(filename).toString(), '01234');
}

// Asynchronous test.
{
  fs.writeFileSync(filename, '0123456789');
  assert.strictEqual(fs.readFileSync(filename).toString(), '0123456789');
  fs.truncate(filename, 5, common.mustCall(function(err) {
    assert.ifError(err);
    assert.strictEqual(fs.readFileSync(filename).toString(), '01234');
  }));
}
