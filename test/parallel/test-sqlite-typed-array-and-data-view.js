'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

const arrayBuffer = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]).buffer;
const sharedArrayBuffer = new SharedArrayBuffer(8);
const typedArrayOnSharedArrayBuffer = new Uint8Array(sharedArrayBuffer);
typedArrayOnSharedArrayBuffer.set([1, 2, 3, 4, 5, 6, 7, 8]);

const TypedArrays = [
  ['Int8Array', Int8Array],
  ['Uint8Array', Uint8Array],
  ['Uint8ClampedArray', Uint8ClampedArray],
  ['Int16Array', Int16Array],
  ['Uint16Array', Uint16Array],
  ['Int32Array', Int32Array],
  ['Uint32Array', Uint32Array],
  ['Float32Array', Float32Array],
  ['Float64Array', Float64Array],
  ['BigInt64Array', BigInt64Array],
  ['BigUint64Array', BigUint64Array],
  ['DataView', DataView],
];

suite('StatementSync with TypedArray/DataView', () => {
  for (const [displayName, TypedArray] of TypedArrays) {
    test(displayName, (t) => {
      const db = new DatabaseSync(nextDb());
      t.after(() => { db.close(); });
      db.exec('CREATE TABLE test (data BLOB)');
      // insert
      {
        const stmt = db.prepare('INSERT INTO test VALUES (?)');
        stmt.run(new TypedArray(arrayBuffer));
      }
      // select all
      {
        const stmt = db.prepare('SELECT * FROM test');
        const row = stmt.get();
        t.assert.ok(row.data instanceof Uint8Array);
        t.assert.strictEqual(row.data.length, 8);
        t.assert.deepStrictEqual(row.data, new Uint8Array(arrayBuffer));
      }
      // query
      {
        const stmt = db.prepare('SELECT * FROM test WHERE data = ?');
        const rows = stmt.all(new TypedArray(arrayBuffer));
        t.assert.strictEqual(rows.length, 1);
        t.assert.ok(rows[0].data instanceof Uint8Array);
        t.assert.strictEqual(rows[0].data.length, 8);
        t.assert.deepStrictEqual(rows[0].data, new Uint8Array(arrayBuffer));
      }
    });
  }
});

suite('StatementSync with ArrayBuffer and SharedArrayBuffer', () => {
  const buffers = [
    ['ArrayBuffer', arrayBuffer],
    ['SharedArrayBuffer', sharedArrayBuffer],
  ];

  for (const [displayName, buffer] of buffers) {
    test(`${displayName} - anonymous binding`, (t) => {
      const db = new DatabaseSync(nextDb());
      t.after(() => { db.close(); });
      db.exec('CREATE TABLE test (data BLOB)');
      // insert
      {
        const stmt = db.prepare('INSERT INTO test VALUES (?)');
        stmt.run(buffer);
      }
      // select all
      {
        const stmt = db.prepare('SELECT * FROM test');
        const row = stmt.get();
        t.assert.ok(row.data instanceof Uint8Array);
        t.assert.strictEqual(row.data.length, 8);
        t.assert.deepStrictEqual(row.data, new Uint8Array(arrayBuffer));
      }
      // query
      {
        const stmt = db.prepare('SELECT * FROM test WHERE data = ?');
        const rows = stmt.all(buffer);
        t.assert.strictEqual(rows.length, 1);
        t.assert.ok(rows[0].data instanceof Uint8Array);
        t.assert.strictEqual(rows[0].data.length, 8);
        t.assert.deepStrictEqual(rows[0].data, new Uint8Array(arrayBuffer));
      }
    });

    test(`${displayName} - named binding (object)`, (t) => {
      const db = new DatabaseSync(nextDb());
      t.after(() => { db.close(); });
      db.exec('CREATE TABLE test (data BLOB)');
      // insert
      {
        const stmt = db.prepare('INSERT INTO test VALUES ($data)');
        stmt.run({ '$data': buffer });
      }
      // select all
      {
        const stmt = db.prepare('SELECT * FROM test');
        const row = stmt.get();
        t.assert.ok(row.data instanceof Uint8Array);
        t.assert.strictEqual(row.data.length, 8);
        t.assert.deepStrictEqual(row.data, new Uint8Array(arrayBuffer));
      }
      // query
      {
        const stmt = db.prepare('SELECT * FROM test WHERE data = $data');
        const rows = stmt.all({ '$data': buffer });
        t.assert.strictEqual(rows.length, 1);
        t.assert.ok(rows[0].data instanceof Uint8Array);
        t.assert.strictEqual(rows[0].data.length, 8);
        t.assert.deepStrictEqual(rows[0].data, new Uint8Array(arrayBuffer));
      }
    });
  }
});
