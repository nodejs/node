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

  test('invalidates prepared statements', () => {
    const db = new DatabaseSync(nextDb());
    db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER);
      INSERT INTO data (key, val) VALUES (1, 2);
    `);

    const select = db.prepare('SELECT * FROM data');
    const insert = db.prepare('INSERT INTO data (key, val) VALUES (?, ?)');

    db[Symbol.dispose]();
    assert.strictEqual(db.isOpen, false);

    for (const method of ['prepare', 'exec']) {
      assert.throws(() => {
        db[method]('SELECT 1');
      }, {
        code: 'ERR_INVALID_STATE',
        message: /database is not open/,
      });
    }

    assert.throws(() => {
      select.get();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
    assert.throws(() => {
      select.all();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
    assert.throws(() => {
      insert.run(2, 4);
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});
