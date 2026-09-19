'use strict';

const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const assert = require('node:assert');
const { test } = require('node:test');
const { DatabaseSync } = require('node:sqlite');

for (const method of ['all', 'get', 'run', 'iterate']) {
  test(`database.close() from a UDF during statement.${method}()`, () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE data (value INTEGER);
      INSERT INTO data VALUES (1), (2), (3);
    `);

    db.function('close_db', (value) => {
      db.close();
      return value;
    });

    const statement = db.prepare('SELECT close_db(value) FROM data');
    assert.throws(() => {
      if (method === 'iterate') {
        for (const row of statement.iterate()) {
          assert.ok(row);
        }
      } else {
        statement[method]();
      }
    }, {
      code: 'ERR_INVALID_STATE',
      message: 'database cannot be closed while in a callback',
    });

    assert.strictEqual(db.isOpen, true);
    db.close();
  });

  // Finalizing the statement being stepped frees the virtual machine that
  // sqlite3_step() is still running, so this must throw rather than crash.
  test(`statement.close() from a UDF during statement.${method}()`, () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE data (value INTEGER);
      INSERT INTO data VALUES (1), (2), (3);
    `);

    let statement;
    db.function('close_stmt', (value) => {
      statement.close();
      return value;
    });

    statement = db.prepare('SELECT close_stmt(value) FROM data');
    assert.throws(() => {
      if (method === 'iterate') {
        for (const row of statement.iterate()) {
          assert.ok(row);
        }
      } else {
        statement[method]();
      }
    }, {
      code: 'ERR_INVALID_STATE',
      message: 'statement is already being executed',
    });

    db.close();
  });

  // Re-running the statement being stepped resets its virtual machine
  // mid-execution, which is the same use-after-free as finalizing it.
  for (const reentrant of ['run', 'get', 'all', 'iterate']) {
    test(`statement.${reentrant}() from a UDF during ` +
         `statement.${method}()`, () => {
      const db = new DatabaseSync(':memory:');
      db.exec(`
        CREATE TABLE data (value INTEGER, padding TEXT);
        INSERT INTO data VALUES (1, '${'x'.repeat(400)}'),
                                (2, '${'y'.repeat(400)}'),
                                (3, '${'z'.repeat(400)}');
      `);

      let statement;
      let thrown;
      db.function('reenter', (value) => {
        if (thrown === undefined) {
          try {
            statement[reentrant]();
            thrown = null;
          } catch (err) {
            thrown = err;
          }
        }
        return value;
      });

      statement = db.prepare('SELECT reenter(value), padding FROM data');
      if (method === 'iterate') {
        for (const row of statement.iterate()) {
          assert.ok(row);
        }
      } else {
        statement[method]();
      }

      assert.ok(thrown, `${reentrant}() was not rejected`);
      assert.strictEqual(thrown.code, 'ERR_INVALID_STATE');
      assert.strictEqual(thrown.message, 'statement is already being executed');

      db.close();
    });
  }

  // Tag store methods resolve to a cached statement, which may be the one
  // currently being stepped. Each reentrant method has its own guard, so all
  // four are exercised.
  for (const reentrant of ['run', 'get', 'all', 'iterate']) {
    test(`tag store ${reentrant} reentry during statement.${method}()`, () => {
      const db = new DatabaseSync(':memory:');
      const sql = db.createTagStore(10);
      db.exec(`
        CREATE TABLE data (value INTEGER, padding TEXT);
        INSERT INTO data VALUES (1, '${'x'.repeat(400)}'),
                                (2, '${'y'.repeat(400)}');
      `);

      let thrown;
      db.function('reenter_tag', (value) => {
        if (thrown === undefined) {
          try {
            // The identical tagged literal resolves to the same cached
            // statement that is mid-execution.
            // All four reject at call time, iterate() included, so the
            // result is never consumed.
            // eslint-disable-next-line no-unused-expressions
            sql[reentrant]`SELECT reenter_tag(value), padding FROM data`;
            thrown = null;
          } catch (err) {
            thrown = err;
          }
        }
        return value;
      });

      if (method === 'iterate') {
        for (const row of sql.iterate`SELECT reenter_tag(value), padding FROM data`) {
          assert.ok(row);
        }
      } else {
        // eslint-disable-next-line no-unused-expressions
        sql[method]`SELECT reenter_tag(value), padding FROM data`;
      }

      assert.ok(thrown, `tag store ${reentrant} reentry was not rejected`);
      assert.strictEqual(thrown.code, 'ERR_INVALID_STATE');
      assert.strictEqual(thrown.message,
                         'statement is already being executed');

      db.close();
    });
  }

  // A UDF may prepare and finalize its own helper statements. Only the
  // statement being stepped is off limits.
  test(`UDF finalizes its own statement during statement.${method}()`, () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE data (value INTEGER);
      INSERT INTO data VALUES (1), (2), (3);
      CREATE TABLE lookup (key INTEGER, label TEXT);
      INSERT INTO lookup VALUES (1, 'one'), (2, 'two'), (3, 'three');
    `);

    db.function('lookup_label', (value) => {
      const helper = db.prepare('SELECT label FROM lookup WHERE key = ?');
      const label = helper.get(value).label;
      helper.close();
      return label;
    });

    const statement = db.prepare('SELECT lookup_label(value) AS l FROM data');
    if (method === 'iterate') {
      const labels = [];
      for (const row of statement.iterate()) {
        labels.push(row.l);
      }
      assert.deepStrictEqual(labels, ['one', 'two', 'three']);
    } else if (method === 'all') {
      assert.deepStrictEqual(statement.all().map((r) => r.l),
                             ['one', 'two', 'three']);
    } else if (method === 'get') {
      assert.strictEqual(statement.get().l, 'one');
    } else {
      statement.run();
    }

    db.close();
  });
}

// iterator.return() resets the statement it is iterating, and next() steps it
// again, so both reach the virtual machine that is mid-execution.
for (const op of ['next', 'return']) {
  test(`iterator.${op}() from a UDF during iteration`, () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE data (value INTEGER, padding TEXT);
      INSERT INTO data VALUES (1, '${'x'.repeat(400)}'),
                              (2, '${'y'.repeat(400)}'),
                              (3, '${'z'.repeat(400)}');
    `);

    let iterator;
    let thrown;
    db.function('reenter_iter', (value) => {
      if (thrown === undefined && iterator !== undefined) {
        try {
          iterator[op]();
          thrown = null;
        } catch (err) {
          thrown = err;
        }
      }
      return value;
    });

    const statement = db.prepare(
      'SELECT reenter_iter(value) AS v, padding FROM data');
    iterator = statement.iterate();
    for (const row of iterator) {
      assert.ok(row);
    }

    assert.ok(thrown, `iterator.${op}() was not rejected`);
    assert.strictEqual(thrown.code, 'ERR_INVALID_STATE');
    assert.strictEqual(thrown.message, 'statement is already being executed');

    db.close();
  });
}
