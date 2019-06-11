'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

[false, 1, [], {}, null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.rmdir(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.rmdirSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});

const d = path.join(tmpdir.path, 'dir', 'test_rmdir_typecheck');
// Make sure the directory does not exist
assert(!fs.existsSync(d));
// Create the directory now
fs.mkdirSync(d, { recursive: true });

// Tests for clearDir option
['true', 1, [], {}].forEach((i) => {
  common.expectsError(
    () => fs.rmdirSync(d, { clearDir: i }),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.rmdir(d, { clearDir: i }, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  assert.rejects(
    fs.promises.rmdir(d, { clearDir: i }),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "clearDir" argument must be of type boolean. ' +
               `Received type ${typeof i}`
    }
  );
});
