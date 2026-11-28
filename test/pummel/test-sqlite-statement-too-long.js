'use strict';

const { enoughTestMem, mustNotCall, skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const { Database, constants: { SQLITE_OK } } = require('node:sqlite');
const { constants } = require('node:buffer');
const dc = require('node:diagnostics_channel');
const { suite, test } = require('node:test');

// These queries allocate strings and blobs of several hundred megabytes.
// Run them separately from the ordinary statement tests.
suite('values larger than the maximum string length', { skip: !enoughTestMem }, () => {
  // hex() doubles its input, so this is the smallest blob whose text form
  // exceeds what V8 can hold in a string.
  const blobSize = (constants.MAX_STRING_LENGTH >>> 1) + 1;
  // '\u20ac' is 1 UTF-16 code unit but 3 UTF-8 bytes, so this identifier is a
  // valid JS string whose UTF-8 form exceeds the limit.
  const longIdentifier = '\u20ac'.repeat(Math.ceil(constants.MAX_STRING_LENGTH / 3) + 10);
  const tooLong = { code: 'ERR_STRING_TOO_LONG', name: 'Error' };

  test('get() throws instead of returning undefined', (t) => {
    using db = new Database(':memory:');
    using stmt = db.prepare('SELECT hex(zeroblob(?))');
    t.assert.throws(() => {
      stmt.get(blobSize);
    }, tooLong);
  });

  test('prepare() throws when the SQLite error message is too long', (t) => {
    using db = new Database(':memory:');
    // SQLite repeats the identifier in the error.
    t.assert.throws(() => {
      db.prepare(`SELECT 1 FROM "${longIdentifier}"`);
    }, tooLong);
  });

  test('prepare() throws for an oversized authorizer argument', (t) => {
    using db = new Database(':memory:');
    db.setAuthorizer(() => SQLITE_OK);
    t.assert.throws(() => db.prepare(`CREATE TABLE "${longIdentifier}" (x)`), tooLong);
  });

  test('columns() throws for an oversized declared type', (t) => {
    using db = new Database(':memory:');
    db.exec(`CREATE TABLE t (x "${longIdentifier}")`);
    using stmt = db.prepare('SELECT x FROM t');
    t.assert.throws(() => stmt.columns(), tooLong);
  });

  test('exec() surfaces the error from a user-defined function', (t) => {
    using db = new Database(':memory:');
    db.exec('CREATE TABLE data(val TEXT)');
    db.function('identity', (val) => val);

    t.assert.throws(() => {
      db.exec(`INSERT INTO data (val) VALUES (identity(hex(zeroblob(${blobSize}))))`);
    }, tooLong);
    using stmt = db.prepare('SELECT count(*) AS count FROM data');
    t.assert.deepStrictEqual(stmt.get(), { __proto__: null, count: 0 });
  });

  test('sourceSQL, expandedSQL and column names throw for oversized SQL', (t) => {
    using db = new Database(':memory:');
    using stmt = db.prepare(`SELECT 1 AS "${longIdentifier}"`);
    t.assert.throws(() => stmt.sourceSQL, tooLong);
    t.assert.throws(() => stmt.expandedSQL, tooLong);
    t.assert.throws(() => stmt.get(), tooLong);
    t.assert.throws(() => stmt.columns(), tooLong);
  });

  test('sqlite.db.query skips statements with oversized SQL', (t) => {
    const onQuery = mustNotCall();
    dc.subscribe('sqlite.db.query', onQuery);
    t.after(() => dc.unsubscribe('sqlite.db.query', onQuery));

    using db = new Database(':memory:');
    using stmt = db.prepare(`SELECT 1 AS "${longIdentifier}"`);
    stmt.run();
    // Make sqlite3_expanded_sql() return NULL to test the source SQL fallback.
    db.limits.length = constants.MAX_STRING_LENGTH;
    stmt.run();
  });
});
