'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();

// This test exercises database.openBlob() and the BlobHandle it returns: the
// options it accepts, the ranges it allows, what happens when the value or the
// connection goes away underneath a handle, and what an option accessor that
// runs user code can and cannot do to an in-flight transfer.

const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { BlobHandle, DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');

tmpdir.refresh();

let dbCount = 0;

function nextDbPath() {
  return join(tmpdir.path, `blob-${dbCount++}.db`);
}

function makeDb(size = 16) {
  const db = new DatabaseSync(':memory:');
  db.exec('CREATE TABLE files (name TEXT, data BLOB)');
  const { lastInsertRowid } = db
    .prepare('INSERT INTO files (name, data) VALUES (?, zeroblob(?))')
    .run('a.bin', size);
  return { db, row: lastInsertRowid };
}

suite('DatabaseSync.prototype.openBlob()', () => {
  test('returns a BlobHandle for an existing value', (t) => {
    const { db, row } = makeDb(32);
    const blob = db.openBlob({ table: 'files', column: 'data', row });
    t.assert.ok(blob instanceof BlobHandle);
    t.assert.strictEqual(blob.byteLength, 32);
    blob.close();
    db.close();
  });

  test('accepts a bigint rowid', (t) => {
    const { db, row } = makeDb();
    const blob = db.openBlob({
      table: 'files',
      column: 'data',
      row: BigInt(row),
    });
    t.assert.strictEqual(blob.byteLength, 16);
    blob.close();
    db.close();
  });

  test('opens a value in an attached database', (t) => {
    const { db, row } = makeDb();
    db.exec('ATTACH DATABASE \':memory:\' AS other');
    db.exec('CREATE TABLE other.files (data BLOB)');
    db.prepare('INSERT INTO other.files VALUES (zeroblob(8))').run();
    const blob = db.openBlob({
      dbName: 'other',
      table: 'files',
      column: 'data',
      row,
    });
    t.assert.strictEqual(blob.byteLength, 8);
    blob.close();
    db.close();
  });

  test('throws if the database is not open', (t) => {
    const { db, row } = makeDb();
    db.close();
    t.assert.throws(() => {
      db.openBlob({ table: 'files', column: 'data', row });
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if options is not an object', (t) => {
    const { db } = makeDb();
    for (const options of [undefined, null, 'files', 5]) {
      t.assert.throws(() => {
        db.openBlob(options);
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options" argument must be an object/,
      });
    }
    db.close();
  });

  test('validates individual options', (t) => {
    const { db, row } = makeDb();
    const base = { table: 'files', column: 'data', row };

    t.assert.throws(() => {
      db.openBlob({ ...base, table: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.table" argument must be a string/,
    });
    t.assert.throws(() => {
      db.openBlob({ ...base, column: undefined });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.column" argument must be a string/,
    });
    t.assert.throws(() => {
      db.openBlob({ ...base, row: '1' });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.row" argument must be a number or a BigInt/,
    });
    t.assert.throws(() => {
      db.openBlob({ ...base, row: 1.5 });
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /The "options\.row" argument must be a safe integer/,
    });
    t.assert.throws(() => {
      db.openBlob({ ...base, readOnly: 'yes' });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.readOnly" argument must be a boolean/,
    });
    t.assert.throws(() => {
      db.openBlob({ ...base, dbName: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.dbName" argument must be a string/,
    });
    db.close();
  });

  test('surfaces SQLite errors for unusable rows and columns', (t) => {
    const { db, row } = makeDb();
    t.assert.throws(() => {
      db.openBlob({ table: 'files', column: 'data', row: 9999 });
    }, { code: 'ERR_SQLITE_ERROR', message: /no such rowid/ });
    t.assert.throws(() => {
      db.openBlob({ table: 'files', column: 'nope', row });
    }, { code: 'ERR_SQLITE_ERROR', message: /no such column/ });
    t.assert.throws(() => {
      db.openBlob({ table: 'nope', column: 'data', row });
    }, { code: 'ERR_SQLITE_ERROR' });

    db.exec('CREATE TABLE wr (k TEXT PRIMARY KEY, d BLOB) WITHOUT ROWID');
    db.prepare('INSERT INTO wr VALUES (?, zeroblob(8))').run('x');
    t.assert.throws(() => {
      db.openBlob({ table: 'wr', column: 'd', row: 1 });
    }, { code: 'ERR_SQLITE_ERROR', message: /without rowid/ });

    db.exec('CREATE TABLE ix (d BLOB UNIQUE)');
    db.prepare('INSERT INTO ix VALUES (zeroblob(8))').run();
    t.assert.throws(() => {
      db.openBlob({ table: 'ix', column: 'd', row: 1 });
    }, { code: 'ERR_SQLITE_ERROR', message: /indexed column/ });
    const blob = db.openBlob({
      table: 'ix',
      column: 'd',
      row: 1,
      readOnly: true,
    });
    t.assert.strictEqual(blob.byteLength, 8);
    blob.close();

    db.exec('CREATE VIRTUAL TABLE vt USING fts5(d)');
    db.exec("INSERT INTO vt VALUES ('value')");
    t.assert.throws(() => {
      db.openBlob({ table: 'vt', column: 'd', row: 1, readOnly: true });
    }, { code: 'ERR_SQLITE_ERROR', message: /cannot open virtual table/ });

    db.exec(`
      CREATE TABLE generated (
        d BLOB,
        size INTEGER GENERATED ALWAYS AS (length(d))
      )
    `);
    db.exec('INSERT INTO generated (d) VALUES (zeroblob(8))');
    t.assert.throws(() => {
      db.openBlob({
        table: 'generated',
        column: 'd',
        row: 1,
        readOnly: true,
      });
    }, { code: 'ERR_SQLITE_ERROR', message: /generated columns/ });
    db.close();
  });

  test('cannot be called from an authorizer callback', (t) => {
    const { db, row } = makeDb();
    let error;
    db.setAuthorizer(() => {
      try {
        db.openBlob({ table: 'files', column: 'data', row });
      } catch (err) {
        error = err;
      }
      return 0;
    });
    db.prepare('SELECT 1').get();
    t.assert.strictEqual(error.code, 'ERR_INVALID_STATE');
    t.assert.match(error.message, /authorizer callback/);
    db.close();
  });

  test('refuses a writable handle on a foreign key child column', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE parents (k BLOB PRIMARY KEY)');
    db.exec('CREATE TABLE children (fk BLOB REFERENCES parents(k))');
    db.prepare('INSERT INTO parents VALUES (zeroblob(8))').run();
    db.prepare('INSERT INTO children VALUES (zeroblob(8))').run();
    // The column carries no index of its own, so the refusal below can only
    // come from the foreign key rule.
    t.assert.deepStrictEqual(
      db.prepare('PRAGMA index_list(children)').all(),
      [],
    );

    t.assert.throws(() => {
      db.openBlob({ table: 'children', column: 'fk', row: 1 });
    }, {
      code: 'ERR_SQLITE_ERROR',
      errcode: 1,
      message: /foreign key/,
    });

    const readOnly = db.openBlob({
      table: 'children',
      column: 'fk',
      row: 1,
      readOnly: true,
    });
    t.assert.strictEqual(readOnly.byteLength, 8);
    readOnly.close();
    db.close();
  });

  test('opens a foreign key child column without enforcement', (t) => {
    // The same schema and the same rows as above: only the pragma moves.
    const db = new DatabaseSync(':memory:', {
      enableForeignKeyConstraints: false,
    });
    db.exec('CREATE TABLE parents (k BLOB PRIMARY KEY)');
    db.exec('CREATE TABLE children (fk BLOB REFERENCES parents(k))');
    db.prepare('INSERT INTO parents VALUES (zeroblob(8))').run();
    db.prepare('INSERT INTO children VALUES (zeroblob(8))').run();
    const blob = db.openBlob({ table: 'children', column: 'fk', row: 1 });
    t.assert.strictEqual(blob.write(Buffer.from([1, 2, 3, 4])), 4);
    blob.close();
    db.close();
  });
});

suite('BlobHandle', () => {
  test('cannot be constructed directly', (t) => {
    t.assert.throws(() => {
      new BlobHandle();
    }, { code: 'ERR_ILLEGAL_CONSTRUCTOR' });
  });

  test('round-trips a value written in chunks', (t) => {
    const { db, row } = makeDb(32);
    {
      using blob = db.openBlob({ table: 'files', column: 'data', row });
      t.assert.strictEqual(blob.write(Buffer.alloc(16, 0xaa)), 16);
      t.assert.strictEqual(
        blob.write(Buffer.alloc(16, 0xbb), { position: 16 }),
        16,
      );
    }

    const expected = Buffer.concat([
      Buffer.alloc(16, 0xaa),
      Buffer.alloc(16, 0xbb),
    ]);
    const { data } = db
      .prepare('SELECT data FROM files WHERE rowid = ?')
      .get(row);
    t.assert.deepStrictEqual(Buffer.from(data), expected);

    using blob = db.openBlob({
      table: 'files',
      column: 'data',
      row,
      readOnly: true,
    });
    const actual = Buffer.alloc(32);
    for (let position = 0; position < blob.byteLength; position += 8) {
      t.assert.strictEqual(
        blob.read(actual, { offset: position, length: 8, position }),
        8,
      );
    }
    t.assert.deepStrictEqual(actual, expected);
    db.close();
  });

  test('reads and writes TypedArrays and DataViews', (t) => {
    const { db, row } = makeDb(8);
    using blob = db.openBlob({ table: 'files', column: 'data', row });
    blob.write(new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]));

    const u8 = new Uint8Array(8);
    t.assert.strictEqual(blob.read(u8), 8);
    t.assert.deepStrictEqual(u8, new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]));

    const view = new DataView(new ArrayBuffer(8));
    t.assert.strictEqual(blob.read(view), 8);
    t.assert.strictEqual(view.getUint8(0), 1);

    // A view onto part of a buffer only sees its own range.
    const backing = new Uint8Array(16).fill(0xff);
    t.assert.strictEqual(blob.read(backing.subarray(8)), 8);
    t.assert.deepStrictEqual(backing.subarray(0, 8), new Uint8Array(8).fill(0xff));
    t.assert.deepStrictEqual(
      backing.subarray(8),
      new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]),
    );
    db.close();
  });

  test('accepts bigint range options', (t) => {
    const { db, row } = makeDb(8);
    using blob = db.openBlob({ table: 'files', column: 'data', row });
    blob.write(Buffer.from([1, 2, 3, 4, 5, 6, 7, 8]));
    const buf = Buffer.alloc(4);
    t.assert.strictEqual(blob.read(buf, { position: 4n, length: 4n }), 4);
    t.assert.deepStrictEqual(buf, Buffer.from([5, 6, 7, 8]));
    db.close();
  });

  test('rejects a buffer that is not a view', (t) => {
    const { db, row } = makeDb();
    using blob = db.openBlob({ table: 'files', column: 'data', row });
    for (const buffer of [undefined, null, 'abc', [1, 2, 3],
                          new ArrayBuffer(8)]) {
      for (const method of ['read', 'write']) {
        t.assert.throws(() => {
          blob[method](buffer);
        }, {
          code: 'ERR_INVALID_ARG_TYPE',
          message: /The "buffer" argument must be a TypedArray or a DataView/,
        });
      }
    }
    db.close();
  });

  test('validates the range options', (t) => {
    const { db, row } = makeDb(16);
    using blob = db.openBlob({ table: 'files', column: 'data', row });
    const buf = Buffer.alloc(8);

    t.assert.throws(() => {
      blob.read(buf, null);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options" argument must be an object/,
    });
    t.assert.throws(() => {
      blob.read(buf, { offset: -1 });
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /The "options\.offset" argument must be >= 0 and <= 8/,
    });
    t.assert.throws(() => {
      blob.read(buf, { offset: 9 });
    }, { code: 'ERR_OUT_OF_RANGE' });
    t.assert.throws(() => {
      blob.read(buf, { length: 9 });
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /The "options\.length" argument must be >= 0 and <= 8/,
    });
    t.assert.throws(() => {
      blob.read(buf, { position: -1 });
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /The "options\.position" argument must be >= 0/,
    });
    t.assert.throws(() => {
      blob.read(buf, { position: 17 });
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /The "options\.position" argument must be >= 0 and <= 16/,
    });
    t.assert.throws(() => {
      blob.read(buf, { position: '0' });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.position" argument must be a number or a BigInt/,
    });
    t.assert.throws(() => {
      blob.read(buf, { position: 1.5 });
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /The "options\.position" argument must be a safe integer/,
    });
    db.close();
  });

  test('reads and writes are all-or-nothing', (t) => {
    const { db, row } = makeDb(16);
    using blob = db.openBlob({ table: 'files', column: 'data', row });
    // Unlike filehandle.read(), there is no short read at the end.
    for (const method of ['read', 'write']) {
      t.assert.throws(() => {
        blob[method](Buffer.alloc(8), { position: 12 });
      }, {
        code: 'ERR_OUT_OF_RANGE',
        message: /extends past the end of the blob/,
      });
    }
    // A request that ends exactly at the end of the value is fine.
    t.assert.strictEqual(blob.read(Buffer.alloc(8), { position: 8 }), 8);
    db.close();
  });

  test('cannot write through a read-only handle', (t) => {
    const { db, row } = makeDb();
    using blob = db.openBlob({
      table: 'files',
      column: 'data',
      row,
      readOnly: true,
    });
    t.assert.throws(() => {
      blob.write(Buffer.alloc(4));
    }, { code: 'ERR_SQLITE_ERROR', message: /readonly/ });
    db.close();
  });

  test('reopen() moves the handle to another row', (t) => {
    const { db, row } = makeDb(16);
    const { lastInsertRowid: other } = db
      .prepare('INSERT INTO files (name, data) VALUES (?, zeroblob(?))')
      .run('b.bin', 64);

    using blob = db.openBlob({ table: 'files', column: 'data', row });
    t.assert.strictEqual(blob.byteLength, 16);
    blob.write(Buffer.alloc(16, 0x01));
    blob.reopen(other);
    t.assert.strictEqual(blob.byteLength, 64);
    blob.write(Buffer.alloc(64, 0x02));

    const rows = db.prepare('SELECT data FROM files ORDER BY rowid').all();
    t.assert.deepStrictEqual(Buffer.from(rows[0].data), Buffer.alloc(16, 0x01));
    t.assert.deepStrictEqual(Buffer.from(rows[1].data), Buffer.alloc(64, 0x02));
    db.close();
  });

  test('reopen() validates its argument', (t) => {
    const { db, row } = makeDb();
    const blob = db.openBlob({ table: 'files', column: 'data', row });
    t.assert.throws(() => {
      blob.reopen('1');
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "row" argument must be a number or a BigInt/,
    });
    // Argument validation does not call sqlite3_blob_reopen() and therefore
    // does not abort the handle.
    t.assert.strictEqual(blob.byteLength, 16);
    t.assert.strictEqual(blob.read(Buffer.alloc(4)), 4);
    blob.close();
    db.close();
  });

  test('a handle is aborted when its row is modified', (t) => {
    const { db, row } = makeDb(16);
    const blob = db.openBlob({
      table: 'files',
      column: 'data',
      row,
      readOnly: true,
    });
    db.prepare('UPDATE files SET name = ? WHERE rowid = ?').run('b.bin', row);

    // Modifying the row expires the handle without changing the byte count
    // cached by SQLite for the value that was originally opened.
    t.assert.strictEqual(blob.byteLength, 16);

    // The first I/O detects the expiration and aborts the handle.
    t.assert.throws(() => {
      blob.read(Buffer.alloc(4));
    }, { code: 'ERR_SQLITE_ERROR', errcode: 4, message: /aborted/ });
    t.assert.strictEqual(blob.byteLength, 0);

    // Once aborted, every data operation keeps reporting SQLITE_ABORT.
    t.assert.throws(() => {
      blob.read(Buffer.alloc(4));
    }, { code: 'ERR_SQLITE_ERROR', errcode: 4, message: /aborted/ });
    t.assert.throws(() => {
      blob.write(Buffer.alloc(4));
    }, { code: 'ERR_SQLITE_ERROR', errcode: 4 });
    t.assert.throws(() => {
      blob.reopen(row);
    }, { code: 'ERR_SQLITE_ERROR', errcode: 4 });
    blob.close();
    db.close();
  });

  test('reopen() can recover an expired handle before I/O', (t) => {
    const { db, row } = makeDb(16);
    const blob = db.openBlob({ table: 'files', column: 'data', row });
    db.prepare('UPDATE files SET data = zeroblob(8) WHERE rowid = ?').run(row);

    // The expired handle still has its old cached size until I/O detects the
    // expiration, so reopening it first can retarget it to the replacement.
    t.assert.strictEqual(blob.byteLength, 16);
    blob.reopen(row);
    t.assert.strictEqual(blob.byteLength, 8);
    t.assert.strictEqual(blob.read(Buffer.alloc(8)), 8);
    blob.close();
    db.close();
  });

  test('a failed reopen() aborts the handle', (t) => {
    const { db, row } = makeDb();
    const blob = db.openBlob({ table: 'files', column: 'data', row });
    t.assert.throws(() => {
      blob.reopen(9999);
    }, { code: 'ERR_SQLITE_ERROR', message: /no such rowid/ });
    // A failed sqlite3_blob_reopen(), rather than row invalidation by itself,
    // is what changes sqlite3_blob_bytes() to zero.
    t.assert.strictEqual(blob.byteLength, 0);
    t.assert.throws(() => {
      blob.read(Buffer.alloc(4));
    }, { code: 'ERR_SQLITE_ERROR', errcode: 4, message: /aborted/ });
    t.assert.throws(() => {
      blob.write(Buffer.alloc(4));
    }, { code: 'ERR_SQLITE_ERROR', errcode: 4 });
    t.assert.throws(() => {
      blob.reopen(row);
    }, { code: 'ERR_SQLITE_ERROR', errcode: 4 });
    blob.close();
    db.close();
  });

  test('close() makes the handle unusable', (t) => {
    const { db, row } = makeDb();
    const blob = db.openBlob({ table: 'files', column: 'data', row });
    blob.close();
    for (const fn of [
      () => blob.byteLength,
      () => blob.read(Buffer.alloc(4)),
      () => blob.write(Buffer.alloc(4)),
      () => blob.reopen(row),
      () => blob.close(),
    ]) {
      t.assert.throws(fn, {
        code: 'ERR_INVALID_STATE',
        message: /blob handle is closed/,
      });
    }
    db.close();
  });

  test('Symbol.dispose is idempotent', (t) => {
    const { db, row } = makeDb();
    const blob = db.openBlob({ table: 'files', column: 'data', row });
    blob[Symbol.dispose]();
    blob[Symbol.dispose]();
    t.assert.throws(() => {
      blob.read(Buffer.alloc(4));
    }, { code: 'ERR_INVALID_STATE' });
    db.close();
  });

  test('closing the database invalidates open handles', (t) => {
    const { db, row } = makeDb();
    const blob = db.openBlob({ table: 'files', column: 'data', row });
    db.close();
    for (const fn of [
      () => blob.byteLength,
      () => blob.read(Buffer.alloc(4)),
      () => blob.close(),
    ]) {
      t.assert.throws(fn, {
        code: 'ERR_INVALID_STATE',
        message: /database is not open/,
      });
    }
  });

  test('an empty value can be opened but not read past', (t) => {
    const { db } = makeDb();
    const { lastInsertRowid } = db
      .prepare('INSERT INTO files (name, data) VALUES (?, zeroblob(0))')
      .run('empty.bin');
    using blob = db.openBlob({
      table: 'files',
      column: 'data',
      row: lastInsertRowid,
    });
    t.assert.strictEqual(blob.byteLength, 0);
    t.assert.strictEqual(blob.read(Buffer.alloc(0)), 0);
    t.assert.throws(() => {
      blob.read(Buffer.alloc(4));
    }, { code: 'ERR_SQLITE_ERROR' });
    db.close();
  });

  test('opens TEXT values as well as BLOB values', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE notes (body TEXT)');
    const { lastInsertRowid } = db
      .prepare('INSERT INTO notes VALUES (?)')
      .run('hello world');
    using blob = db.openBlob({
      table: 'notes',
      column: 'body',
      row: lastInsertRowid,
      readOnly: true,
    });
    t.assert.strictEqual(blob.byteLength, 11);
    const buf = Buffer.alloc(5);
    blob.read(buf, { position: 6 });
    t.assert.strictEqual(buf.toString(), 'world');
    db.close();
  });

  test('writes bypass SQL update semantics', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE files (
        data BLOB CHECK (hex(substr(data, 1, 1)) = '00')
      );
      CREATE TABLE audit (event TEXT);
      CREATE TRIGGER files_updated AFTER UPDATE ON files BEGIN
        INSERT INTO audit VALUES ('updated');
      END;
      INSERT INTO files VALUES (zeroblob(4));
    `);
    const blob = db.openBlob({ table: 'files', column: 'data', row: 1 });
    blob.write(Buffer.from([0xff]));
    blob.close();

    t.assert.strictEqual(
      db.prepare('SELECT hex(data) AS data FROM files').get().data,
      'FF000000',
    );
    t.assert.strictEqual(
      db.prepare('SELECT count(*) AS count FROM audit').get().count,
      0,
    );
    t.assert.strictEqual(
      db.prepare('PRAGMA integrity_check').get().integrity_check,
      'CHECK constraint failed in files',
    );
    db.close();
  });

  test('writes TEXT values as raw bytes', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec("CREATE TABLE notes (body TEXT); INSERT INTO notes VALUES ('text')");
    const blob = db.openBlob({ table: 'notes', column: 'body', row: 1 });
    blob.write(Buffer.from([0xff]));
    blob.close();

    const result = db
      .prepare('SELECT typeof(body) AS type, hex(body) AS body FROM notes')
      .get();
    t.assert.strictEqual(result.type, 'text');
    t.assert.strictEqual(result.body, 'FF657874');
    db.close();
  });

  test('streams a value larger than the buffers used to move it', (t) => {
    const size = 4 * 1024 * 1024;
    const { db } = makeDb();
    const { lastInsertRowid } = db
      .prepare('INSERT INTO files (name, data) VALUES (?, zeroblob(?))')
      .run('big.bin', size);
    const options = { table: 'files', column: 'data', row: lastInsertRowid };
    const chunk = Buffer.alloc(64 * 1024);

    {
      using blob = db.openBlob(options);
      for (let position = 0; position < size; position += chunk.length) {
        chunk.fill((position / chunk.length) & 0xff);
        blob.write(chunk, { position });
      }
    }

    using blob = db.openBlob({ ...options, readOnly: true });
    t.assert.strictEqual(blob.byteLength, size);
    for (let position = 0; position < size; position += chunk.length) {
      blob.read(chunk, { position });
      t.assert.deepStrictEqual(
        chunk,
        Buffer.alloc(chunk.length, (position / chunk.length) & 0xff),
      );
    }
    db.close();
  });
});

suite('BlobHandle range limits', () => {
  test('rejects a range larger than SQLite can address', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE files (data BLOB)');
    db.prepare('INSERT INTO files VALUES (zeroblob(0))').run();
    using blob = db.openBlob({ table: 'files', column: 'data', row: 1 });
    // byteLength is 0, so the blob-relative checks are skipped; the int range
    // of sqlite3_blob_read() still has to be enforced. Only position is
    // reachable here -- length is already bounded by the size of the buffer.
    t.assert.throws(() => {
      blob.read(Buffer.alloc(8), { position: 2n ** 31n });
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /must be <= 2147483647/,
    });
    db.close();
  });
});

suite('BlobHandle reentrancy', () => {
  // Reading the options object can run user code: it may expose accessors or
  // be a Proxy. That code must not be able to invalidate state the operation
  // has already validated.
  function setup(size = 4096) {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (d BLOB)');
    db.prepare('INSERT INTO t VALUES (zeroblob(?))').run(size);
    return { db, blob: db.openBlob({ table: 't', column: 'd', row: 1 }) };
  }

  function optionsThatRun(fn, length) {
    return {
      offset: 0,
      length,
      get position() { fn(); return 0; },
    };
  }

  test('a getter that detaches the buffer cannot cause a bad write', (t) => {
    for (const method of ['read', 'write']) {
      const { db, blob } = setup();
      const buffer = new Uint8Array(4096);
      const options = optionsThatRun(() => {
        structuredClone(buffer.buffer, { transfer: [buffer.buffer] });
      }, 4096);
      t.assert.throws(() => {
        blob[method](buffer, options);
      }, { code: 'ERR_OUT_OF_RANGE' });
      db.close();
    }
  });

  test('a getter that shrinks a resizable buffer cannot overrun it', (t) => {
    const { db, blob } = setup();
    const ab = new ArrayBuffer(4096, { maxByteLength: 4096 });
    const buffer = new Uint8Array(ab);
    t.assert.throws(() => {
      blob.read(buffer, optionsThatRun(() => ab.resize(8), 4096));
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /"options\.length" argument must be >= 0 and <= 8/,
    });
    db.close();
  });

  test('a getter that closes the handle is reported as a closed handle', (t) => {
    const { db, blob } = setup();
    t.assert.throws(() => {
      blob.read(new Uint8Array(16), optionsThatRun(() => blob.close(), 16));
    }, {
      code: 'ERR_INVALID_STATE',
      message: /blob handle is closed/,
    });
    db.close();
  });

  test('a getter that closes the database is reported as a closed db', (t) => {
    const { db, blob } = setup();
    t.assert.throws(() => {
      blob.read(new Uint8Array(16), optionsThatRun(() => db.close(), 16));
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('openBlob() rechecks the database after reading its options', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (d BLOB)');
    db.prepare('INSERT INTO t VALUES (zeroblob(64))').run();
    t.assert.throws(() => {
      db.openBlob({
        table: 't',
        column: 'd',
        get row() { db.close(); return 1; },
      });
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('a Proxy options object is handled the same way', (t) => {
    const { db, blob } = setup();
    const buffer = new Uint8Array(4096);
    const options = new Proxy({}, {
      get(target, prop) {
        if (prop === 'position') {
          structuredClone(buffer.buffer, { transfer: [buffer.buffer] });
          return 0;
        }
        return prop === 'length' ? 4096 : 0;
      },
    });
    t.assert.throws(() => {
      blob.read(buffer, options);
    }, { code: 'ERR_OUT_OF_RANGE' });
    db.close();
  });
});

suite('BlobHandle guards found in review', () => {
  const NUL = String.fromCharCode(0);

  function setup(size = 8) {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (d BLOB)');
    db.prepare('INSERT INTO t VALUES (zeroblob(?))').run(size);
    return db;
  }

  test('identifier options reject embedded null bytes', (t) => {
    const db = setup();
    // SQLite takes these as NUL-terminated C strings, so truncation would
    // silently open a different table, column or database.
    for (const [key, value] of [
      ['table', `t${NUL}nope`],
      ['column', `d${NUL}nope`],
      ['dbName', `main${NUL}nope`],
    ]) {
      t.assert.throws(() => {
        db.openBlob({ table: 't', column: 'd', row: 1, [key]: value });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: new RegExp(`"options\\.${key}" argument must not contain`),
      });
    }
    db.close();
  });

  test('operations are barred from an authorizer callback', (t) => {
    const db = setup();
    const blob = db.openBlob({ table: 't', column: 'd', row: 1 });
    const results = {};
    db.setAuthorizer(() => {
      for (const [name, fn] of [
        ['read', () => blob.read(Buffer.alloc(4))],
        ['write', () => blob.write(Buffer.alloc(4))],
        ['reopen', () => blob.reopen(1)],
        ['close', () => blob.close()],
        ['dispose', () => blob[Symbol.dispose]()],
      ]) {
        try {
          fn();
          results[name] = 'no throw';
        } catch (err) {
          results[name] = err.code;
        }
      }
      return 0;
    });
    db.prepare('SELECT 1').get();
    t.assert.deepStrictEqual(results, {
      read: 'ERR_INVALID_STATE',
      write: 'ERR_INVALID_STATE',
      reopen: 'ERR_INVALID_STATE',
      close: 'ERR_INVALID_STATE',
      dispose: 'ERR_INVALID_STATE',
    });
    db.close();
  });

  test('a zero-length transfer is still checked by SQLite', (t) => {
    const db = setup();
    // A zero-length range transfers nothing, but read-only enforcement and
    // aborted-handle detection still have to apply.
    const readOnly = db.openBlob({
      table: 't',
      column: 'd',
      row: 1,
      readOnly: true,
    });
    t.assert.throws(() => {
      readOnly.write(Buffer.alloc(0));
    }, { code: 'ERR_SQLITE_ERROR', message: /readonly/ });
    readOnly.close();

    const aborted = db.openBlob({ table: 't', column: 'd', row: 1 });
    db.prepare('UPDATE t SET d = zeroblob(8) WHERE rowid = 1').run();
    t.assert.throws(() => {
      aborted.read(Buffer.alloc(0));
    }, { code: 'ERR_SQLITE_ERROR', errcode: 4 });
    aborted.close();
    db.close();
  });

  test('closing does not repeat an error that already threw', (t) => {
    const db = setup();
    const blob = db.openBlob({
      table: 't',
      column: 'd',
      row: 1,
      readOnly: true,
    });
    t.assert.throws(() => {
      blob.write(Buffer.alloc(4));
    }, { code: 'ERR_SQLITE_ERROR' });
    // sqlite3_blob_close() reports the statement's last result code, which is
    // the failure above. It has already been delivered once.
    blob.close();
    db.close();
  });

  test('a BigInt outside the 64-bit range is a range error', (t) => {
    const db = setup();
    for (const row of [2n ** 64n, -(2n ** 64n)]) {
      t.assert.throws(() => {
        db.openBlob({ table: 't', column: 'd', row });
      }, {
        code: 'ERR_OUT_OF_RANGE',
        message: /within the range of a signed 64-bit integer/,
      });
    }
    db.close();
  });

  test('a transfer at a nonzero buffer offset lands at that offset', (t) => {
    const db = setup();
    const blob = db.openBlob({ table: 't', column: 'd', row: 1 });
    blob.write(Buffer.from([1, 2, 3, 4, 5, 6, 7, 8]));
    const buffer = Buffer.alloc(16, 0xff);
    t.assert.strictEqual(blob.read(buffer, { offset: 4, length: 4 }), 4);
    t.assert.deepStrictEqual(buffer.subarray(0, 4), Buffer.alloc(4, 0xff));
    t.assert.deepStrictEqual(buffer.subarray(4, 8), Buffer.from([1, 2, 3, 4]));
    t.assert.deepStrictEqual(buffer.subarray(8), Buffer.alloc(8, 0xff));
    blob.close();
    db.close();
  });
});

suite('BlobHandle deferred commit', () => {
  // A write through a blob handle is committed when the handle is closed, so a
  // failure there belongs to the close rather than to the write that has
  // already returned. A reader on a second connection holds the shared lock
  // that the commit needs, which makes such a failure reachable without an
  // error injecting VFS. The result code is SQLITE_BUSY instead of an I/O
  // error, but it travels the same path.
  function lockedForCommit(t) {
    const path = nextDbPath();
    const writer = new DatabaseSync(path, { timeout: 0 });
    const reader = new DatabaseSync(path, { timeout: 0 });
    t.after(() => {
      for (const db of [reader, writer]) {
        try {
          db.close();
        } catch {
          // Already closed, or closing reported the blocked commit.
        }
      }
    });
    writer.exec('CREATE TABLE files (data BLOB)');
    writer.prepare('INSERT INTO files VALUES (zeroblob(16))').run();
    // A deferred transaction takes no lock until it reads.
    reader.exec('BEGIN');
    reader.prepare('SELECT count(*) FROM files').get();
    return { path, writer, reader };
  }

  test('close reports a commit that could not be taken', (t) => {
    const { writer, reader } = lockedForCommit(t);
    const blob = writer.openBlob({ table: 'files', column: 'data', row: 1 });
    t.assert.strictEqual(blob.write(Buffer.from([1, 2, 3, 4])), 4);
    t.assert.throws(() => {
      blob.close();
    }, {
      code: 'ERR_SQLITE_ERROR',
      errcode: 5,
      message: /database is locked/,
    });
    // The handle is released even though the close reported an error.
    t.assert.throws(() => {
      blob.close();
    }, { code: 'ERR_INVALID_STATE' });

    reader.exec('COMMIT');
    t.assert.strictEqual(
      writer.prepare('SELECT hex(data) AS h FROM files').get().h,
      '0'.repeat(32),
    );
  });

  test('database.close() reports a commit that could not be taken', (t) => {
    const { path, writer, reader } = lockedForCommit(t);
    const blob = writer.openBlob({ table: 'files', column: 'data', row: 1 });
    const otherBlob = writer.openBlob({
      table: 'files',
      column: 'data',
      row: 1,
      readOnly: true,
    });
    blob.write(Buffer.from([1, 2, 3, 4]));
    t.assert.throws(() => {
      writer.close();
    }, { code: 'ERR_SQLITE_ERROR', errcode: 5 });
    t.assert.throws(() => {
      blob.read(Buffer.alloc(4));
    }, { code: 'ERR_INVALID_STATE' });
    t.assert.throws(() => {
      otherBlob.read(Buffer.alloc(4));
    }, { code: 'ERR_INVALID_STATE' });
    t.assert.throws(() => {
      writer.prepare('SELECT 1');
    }, { code: 'ERR_INVALID_STATE', message: /database is not open/ });

    reader.exec('COMMIT');
    const observer = new DatabaseSync(path);
    t.assert.strictEqual(
      observer.prepare('SELECT hex(data) AS h FROM files').get().h,
      '0'.repeat(32),
    );
    observer.close();
  });

  test('Symbol.dispose reports a blocked commit', (t) => {
    const { writer, reader } = lockedForCommit(t);
    const blob = writer.openBlob({ table: 'files', column: 'data', row: 1 });
    blob.write(Buffer.from([1, 2, 3, 4]));
    t.assert.throws(() => {
      blob[Symbol.dispose]();
    }, {
      code: 'ERR_SQLITE_ERROR',
      errcode: 5,
      errstr: 'database is locked',
    });
    blob[Symbol.dispose]();

    reader.exec('COMMIT');
    t.assert.strictEqual(
      writer.prepare('SELECT hex(data) AS h FROM files').get().h,
      '0'.repeat(32),
    );
  });

  test('database Symbol.dispose reports a blocked commit', (t) => {
    const { path, writer, reader } = lockedForCommit(t);
    const blob = writer.openBlob({ table: 'files', column: 'data', row: 1 });
    blob.write(Buffer.from([1, 2, 3, 4]));
    t.assert.throws(() => {
      writer[Symbol.dispose]();
    }, {
      code: 'ERR_SQLITE_ERROR',
      errcode: 5,
    });
    writer[Symbol.dispose]();
    t.assert.throws(() => {
      blob.read(Buffer.alloc(4));
    }, { code: 'ERR_INVALID_STATE' });

    reader.exec('COMMIT');
    const observer = new DatabaseSync(path);
    t.assert.strictEqual(
      observer.prepare('SELECT hex(data) AS h FROM files').get().h,
      '0'.repeat(32),
    );
    observer.close();
  });
});
