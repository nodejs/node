'use strict';
const { skipIfSQLiteMissing } = require('../common/index.mjs');
const { test, suite } = require('node:test');
skipIfSQLiteMissing();
const { DatabaseSync, Database } = require('node:sqlite');

suite('DatabaseSync config', () => {
  function checkDefensiveMode(t, db) {
    function journalMode() {
      return db.prepare('PRAGMA journal_mode').get().journal_mode;
    }

    t.assert.strictEqual(journalMode(), 'memory');
    db.exec('PRAGMA journal_mode=OFF');

    switch (journalMode()) {
      case 'memory': return true; // journal_mode unchanged, defensive mode must be active
      case 'off': return false;  // journal_mode now 'off', so defensive mode not active
      default: throw new Error('unexpected journal_mode');
    }
  }

  test('by default, defensive mode is on', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.strictEqual(checkDefensiveMode(t, db), true);
  });

  test('when passing { defensive: true } as config, defensive mode is on', (t) => {
    const db = new DatabaseSync(':memory:', {
      defensive: true
    });
    t.assert.strictEqual(checkDefensiveMode(t, db), true);
  });

  test('when passing { defensive: false } as config, defensive mode is off', (t) => {
    const db = new DatabaseSync(':memory:', {
      defensive: false
    });
    t.assert.strictEqual(checkDefensiveMode(t, db), false);
  });

  test('defensive mode on after calling db.enableDefensive(true)', (t) => {
    const db = new DatabaseSync(':memory:');
    db.enableDefensive(true);
    t.assert.strictEqual(checkDefensiveMode(t, db), true);
  });

  test('defensive mode off after calling db.enableDefensive(false)', (t) => {
    const db = new DatabaseSync(':memory:', {
      defensive: true
    });
    db.enableDefensive(false);
    t.assert.strictEqual(checkDefensiveMode(t, db), false);
  });

  test('throws if options.defensive is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync(':memory:', { defensive: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.defensive" argument must be a boolean.',
    });
  });
});

suite('Database config', { timeout: 1000 }, () => {
  async function checkDefensiveMode(t, db) {
    async function journalMode() {
      return (await (await db.prepare('PRAGMA journal_mode')).get()).journal_mode;
    }

    t.assert.strictEqual(await journalMode(), 'memory');
    await db.exec('PRAGMA journal_mode=OFF');

    switch (await journalMode()) {
      case 'memory': return true; // journal_mode unchanged, defensive mode must be active
      case 'off': return false;  // journal_mode now 'off', so defensive mode not active
      default: throw new Error('unexpected journal_mode');
    }
  }

  test('by default, defensive mode is on', async (t) => {
    const db = new Database(':memory:');
    t.after(async () => await db.close());
    t.assert.strictEqual(await checkDefensiveMode(t, db), true);
  });

  test('when passing { defensive: true } as config, defensive mode is on', async (t) => {
    const db = new Database(':memory:', {
      defensive: true
    });
    t.after(async () => await db.close());
    t.assert.strictEqual(await checkDefensiveMode(t, db), true);
  });

  test('when passing { defensive: false } as config, defensive mode is off', async (t) => {
    const db = new Database(':memory:', {
      defensive: false
    });
    t.after(async () => await db.close());
    t.assert.strictEqual(await checkDefensiveMode(t, db), false);
  });

  test('defensive mode on after calling db.enableDefensive(true)', async (t) => {
    const db = new Database(':memory:');
    await db.enableDefensive(true);
    t.assert.strictEqual(await checkDefensiveMode(t, db), true);
  });

  test('defensive mode off after calling db.enableDefensive(false)', async (t) => {
    const db = new Database(':memory:', {
      defensive: true
    });
    t.after(async () => await db.close());
    await db.enableDefensive(false);
    t.assert.strictEqual(await checkDefensiveMode(t, db), false);
  });

  test('throws if options.defensive is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new Database(':memory:', { defensive: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.defensive" argument must be a boolean.',
    });
  });
});
