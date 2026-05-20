'use strict';

// Test of fs.readFile with different flags.
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

{
  const emptyFile = tmpdir.resolve('empty.txt');
  fs.closeSync(fs.openSync(emptyFile, 'w'));

  fs.readFile(
    emptyFile,
    // With `a+` the file is created if it does not exist
    common.mustNotMutateObjectDeep({ encoding: 'utf8', flag: 'a+' }),
    common.mustCall((err, data) => { assert.strictEqual(data, ''); })
  );

  fs.readFile(
    emptyFile,
    // Like `a+` but fails if the path exists.
    common.mustNotMutateObjectDeep({ encoding: 'utf8', flag: 'ax+' }),
    common.mustCall((err, data) => { assert.strictEqual(err.code, 'EEXIST'); })
  );
}

{
  const willBeCreated = tmpdir.resolve('will-be-created');

  fs.readFile(
    willBeCreated,
    // With `a+` the file is created if it does not exist
    common.mustNotMutateObjectDeep({ encoding: 'utf8', flag: 'a+' }),
    common.mustCall((err, data) => { assert.strictEqual(data, ''); })
  );
}

{
  const willNotBeCreated = tmpdir.resolve('will-not-be-created');

  fs.readFile(
    willNotBeCreated,
    // Default flag is `r`. An exception occurs if the file does not exist.
    common.mustNotMutateObjectDeep({ encoding: 'utf8' }),
    common.mustCall((err, data) => { assert.strictEqual(err.code, 'ENOENT'); })
  );
}
