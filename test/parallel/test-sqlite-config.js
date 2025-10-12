'use strict';
const { skipIfSQLiteMissing } = require('../common/index.mjs');
const { test } = require('node:test');
const assert = require('node:assert');
const { DatabaseSync } = require('node:sqlite');
skipIfSQLiteMissing();

function checkDefensiveMode(db) {
  function journalMode() {
    return db.prepare('PRAGMA journal_mode').get().journal_mode;
  }

  assert.strictEqual(journalMode(), 'memory');
  db.exec('PRAGMA journal_mode=OFF');

  switch (journalMode()) {
    case 'memory': return true; // journal_mode unchanged, defensive mode must be active
    case 'off': return false;  // journal_mode now 'off', so defensive mode not active
    default: throw new Error('unexpected journal_mode');
  }
}

test('by default, defensive mode is off', (t) => {
  const db = new DatabaseSync(':memory:');
  t.assert.strictEqual(checkDefensiveMode(db), false);
});

test('when passing { defensive: true } as config, defensive mode is on', (t) => {
  const db = new DatabaseSync(':memory:', {
    defensive: true
  });
  t.assert.strictEqual(checkDefensiveMode(db), true);
});

test('defensive mode on after calling db.enableDefensive(true)', (t) => {
  const db = new DatabaseSync(':memory:');
  db.enableDefensive(true);
  t.assert.strictEqual(checkDefensiveMode(db), true);
});

test('defensive mode should be off after calling db.enableDefensive(false)', (t) => {
  const db = new DatabaseSync(':memory:', {
    defensive: true
  });
  db.enableDefensive(false);
  t.assert.strictEqual(checkDefensiveMode(db), false);
});

test('throws if options.defensive is provided but is not a boolean', (t) => {
  t.assert.throws(() => {
    new DatabaseSync(':memory:', { defensive: 42 });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options.defensive" argument must be a boolean.',
  });
});
