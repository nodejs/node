// Direct tests of the prepareDb() statement layer, independent of KVStore.
// Most of this layer is already exercised indirectly through test-kvstore.js,
// but KVStore.getMany() rejects an empty key list itself (ERR_INVALID_ARG_VALUE)
// before ever calling into this layer, so its own early-return guard has no
// path to it through the public API — only a direct test can reach it.
'use strict';

const { skipIfKVStoreMissing } = require('../common');
skipIfKVStoreMissing();

const assert = require('node:assert/strict');
const { DatabaseSync } = require('node:sqlite');
const { describe, it } = require('node:test');
const { encodeKey } = require('internal/kvstore/keys');
const { prepareDb } = require('internal/kvstore/statements');

describe('prepareDb (statement layer)', () => {
  function open() {
    return prepareDb(new DatabaseSync(':memory:'), { isMemory: true });
  }

  it('getMany([]) returns an empty Map-like without querying the database', () => {
    const db = open();
    const result = db.getMany([]);
    assert.strictEqual(result.size, 0);
  });

  it('upsert/get/delete/clear round-trip', () => {
    const db = open();
    const { encoded } = encodeKey(['a']);

    assert.deepEqual(db.get(encoded), { value: null });

    db.upsert(encoded, 'hello');
    assert.deepEqual(db.get(encoded), { value: 'hello' });

    assert.strictEqual(db.delete(encoded).changes, 1);
    assert.deepEqual(db.get(encoded), { value: null });

    db.upsert(encoded, 'again');
    db.clear();
    assert.deepEqual(db.get(encoded), { value: null });
  });
});
