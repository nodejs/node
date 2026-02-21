'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { join } = require('node:path');
const { DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

suite('DatabaseSync.prototype[Symbol.dispose]()', () => {
  test('closes an open database', () => {
    const db = new DatabaseSync(nextDb());
    db[Symbol.dispose]();
    assert.throws(() => {
      db.close();
    }, /database is not open/);
  });

  test('supports databases that are not open', () => {
    const db = new DatabaseSync(nextDb(), { open: false });
    db[Symbol.dispose]();
    assert.throws(() => {
      db.close();
    }, /database is not open/);
  });
});
