'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const filename = common.tmpDir + '/truncate-file.txt';

common.refreshTmpDir();

// Synchronous test.
(function() {
  fs.writeFileSync(filename, '0123456789');
  assert.equal(fs.readFileSync(filename).toString(), '0123456789');
  fs.truncateSync(filename, 5);
  assert.equal(fs.readFileSync(filename).toString(), '01234');
})();

// Asynchronous test.
(function() {
  fs.writeFileSync(filename, '0123456789');
  assert.equal(fs.readFileSync(filename).toString(), '0123456789');
  fs.truncate(filename, 5, common.mustCall(function(err) {
    if (err) throw err;
    assert.equal(fs.readFileSync(filename).toString(), '01234');
  }));
})();
