'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  common.fsTest('access', [Buffer.from(tmpdir.path), assert.ifError]);
}

{
  const buf = Buffer.from(path.join(tmpdir.path, 'a.txt'));
  common.fsTest('open', [buf, 'w+', (err, fd) => {
    assert.ifError(err);
    assert(fd);
    if (typeof fd === 'number')
      fs.close(fd, common.mustCall(assert.ifError));
    else
      fd.close().catch((e) => { setTimeout((e) => { throw e; }, 1); });
  }]);
}

common.expectsError(
  () => {
    fs.accessSync(true);
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "path" argument must be one of type string, Buffer, or URL.' +
             ' Received type boolean'
  }
);

{
  const dir = Buffer.from(fixtures.fixturesDir);
  const results = [];

  const check = common.mustCall((results) => {
    for (let i = 1; i < results.length; i++) {
      assert.deepStrictEqual(results[i], results[0]);
    }
  });

  common.fsTest('readdir', [dir, 'hex', (err, hexList) => {
    assert.ifError(err);
    results.push(hexList.map((val) => Buffer.from(val, 'hex').toString()));
    if (results.length > 3)
      check(results);
  }]);
  common.fsTest('readdir', [dir, (err, stringList) => {
    assert.ifError(err);
    results.push(stringList);
    if (results.length > 3)
      check(results);
  }]);
}
