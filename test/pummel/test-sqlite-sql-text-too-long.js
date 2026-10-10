'use strict';
const common = require('../common');
common.skipIfSQLiteMissing();

if (!common.enoughTestMem)
  common.skip('intensive SQL text tests due to memory confinements');

const assert = require('node:assert');
const { constants } = require('node:buffer');
const dc = require('node:diagnostics_channel');
const { Database } = require('node:sqlite');

// 'é' is one byte per character in V8 and two bytes in UTF-8.
const text = 'é'.repeat((constants.MAX_STRING_LENGTH >>> 1) + 1);
const tooLong = { code: 'ERR_STRING_TOO_LONG', name: 'Error' };

{
  using db = new Database(':memory:');
  using stmt = db.prepare(`SELECT length('${text}') AS len`);
  assert.throws(() => stmt.sourceSQL, tooLong);
}

{
  using db = new Database(':memory:');
  using stmt = db.prepare(`SELECT length('${text}')`);
  assert.throws(() => stmt.get(), tooLong);
  assert.throws(() => stmt.all(), tooLong);
  assert.throws(() => stmt.iterate().next(), tooLong);
  assert.throws(() => stmt.columns(), tooLong);
}

{
  const handler = common.mustNotCall();
  dc.subscribe('sqlite.db.query', handler);
  const sql = `SELECT length('${text}') AS a, length(?) AS b`;
  using db = new Database(':memory:');
  db.limits.length = Buffer.byteLength(text);
  using stmt = db.prepare(sql);
  assert.deepStrictEqual(
    stmt.get(Buffer.alloc(1)),
    { __proto__: null, a: text.length, b: 1 },
  );
  assert.throws(() => stmt.expandedSQL, { code: 'ERR_SQLITE_ERROR' });
  dc.unsubscribe('sqlite.db.query', handler);
}
