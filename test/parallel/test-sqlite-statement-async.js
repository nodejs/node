// Flags: --expose-gc
'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { Database, Statement } = require('node:sqlite');
const { suite, test } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

suite('Statement() constructor', () => {
  test('Statement cannot be constructed directly', (t) => {
    t.assert.throws(() => {
      new Statement();
    }, {
      code: 'ERR_ILLEGAL_CONSTRUCTOR',
      message: /Illegal constructor/,
    });
  });
});
