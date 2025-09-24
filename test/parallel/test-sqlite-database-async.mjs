import { skipIfSQLiteMissing } from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import { suite, test } from 'node:test';
import { join } from 'node:path';
import { Database } from 'node:sqlite';
skipIfSQLiteMissing();

tmpdir.refresh();

let cnt = 0;
function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

suite('Database.prototype.exec()', () => {
  test('executes SQL', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const result = await db.exec(`
      CREATE TABLE data(
        key INTEGER PRIMARY KEY,
        val INTEGER
      ) STRICT;
      INSERT INTO data (key, val) VALUES (1, 2);
      INSERT INTO data (key, val) VALUES (8, 9);
    `);
    t.assert.strictEqual(result, undefined);
  });

  test('reports errors from SQLite', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });

    await t.assert.rejects(db.exec('CREATE TABLEEEE'), {
      code: 'ERR_SQLITE_ERROR',
      message: /syntax error/,
    });
  });

  test('throws if the URL does not have the file: scheme', (t) => {
    t.assert.throws(() => {
      new Database(new URL('http://example.com'));
    }, {
      code: 'ERR_INVALID_URL_SCHEME',
      message: 'The URL must be of scheme file:',
    });
  });

  test('throws if database is not open', (t) => {
    const db = new Database(nextDb(), { open: false });

    t.assert.throws(() => {
      db.exec();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if sql is not a string', (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.exec();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "sql" argument must be a string/,
    });
  });
});
