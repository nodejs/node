'use strict';

const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { join } = require('node:path');
const { test } = require('node:test');
const { backup, DatabaseSync } = require('node:sqlite');

tmpdir.refresh();

const closedError = {
  code: 'ERR_INVALID_STATE',
  message: 'database is not open',
};

// Reading the options bag runs a user getter, so the connection validated on
// entry can already be closed by the time the method reaches SQLite.
test('function() with a getter that closes the database', () => {
  const db = new DatabaseSync(':memory:');
  const options = {
    get useBigIntArguments() {
      db.close();
      return false;
    },
  };

  assert.throws(() => {
    db.function('custom', options, () => 1);
  }, closedError);
});

test('aggregate() with a getter that closes the database', () => {
  const db = new DatabaseSync(':memory:');
  const options = {
    start: 0,
    step: (acc, value) => acc + value,
    get useBigIntArguments() {
      db.close();
      return false;
    },
  };

  assert.throws(() => {
    db.aggregate('custom', options);
  }, closedError);
});

test('deserialize() with a getter that closes the database', () => {
  const source = new DatabaseSync(':memory:');
  source.exec('CREATE TABLE data (value INTEGER)');
  const serialized = source.serialize();
  source.close();

  const db = new DatabaseSync(':memory:');
  const options = {
    get dbName() {
      db.close();
      return 'main';
    },
  };

  assert.throws(() => {
    db.deserialize(serialized, options);
  }, closedError);
});

test('backup() with a getter that closes the database', () => {
  const db = new DatabaseSync(':memory:');
  db.exec('CREATE TABLE data (value INTEGER)');
  const options = {
    get rate() {
      db.close();
      return 1;
    },
  };

  assert.throws(() => {
    backup(db, join(tmpdir.path, 'backup.db'), options);
  }, closedError);
});
