// Flags: --expose-gc --expose-internals
'use strict';

const { mustCall, skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();

const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { DatabaseSync } = require('node:sqlite');
const { suite, it } = require('node:test');
const { gcUntil } = require('../common/gc');
const { internalBinding } = require('internal/test/binding');

// Tracer-driven activation is simulated by writing the USDT probe
// semaphore view, which is only truthful on tiers where it is the live
// kernel value (ordinary Linux). On placeholder tiers (native DTrace,
// sandboxed builds) the view is constant one and the native probeEnabled()
// query alone decides, and on tiers without USDT there is no view at all.
// Coverage with a real tracer runs in the bpftrace harness.
const dcBinding = internalBinding('diagnostics_channel');
const canSimulateTracer = dcBinding.probeSemaphore !== undefined &&
                          dcBinding.probeEnabled === undefined;

suite('sqlite.db.query diagnostics channel', () => {
  it('subscriber receives SQL string for exec() statements', (t) => {
    const calls = [];
    using db = new DatabaseSync(':memory:');

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');

    assert.strictEqual(calls.length, 2);
    assert.strictEqual(calls[0].sql, 'CREATE TABLE t (x INTEGER)');
    assert.strictEqual(calls[1].sql, 'INSERT INTO t VALUES (1)');
  });

  it('subscriber receives SQL string for prepared INSERT statements', (t) => {
    let calls = [];
    using db = new DatabaseSync(':memory:');

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    calls = []; // reset after setup

    using stmt = db.prepare('INSERT INTO t VALUES (?)');
    stmt.run(42);

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0].sql, 'INSERT INTO t VALUES (42.0)');
  });

  it('subscriber receives SQL string for prepared SELECT statements', (t) => {
    let calls = [];
    using db = new DatabaseSync(':memory:');

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    calls = []; // reset after setup

    using stmt = db.prepare('SELECT x FROM t WHERE x = ?');
    stmt.get(1);

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0].sql, 'SELECT x FROM t WHERE x = 1.0');
  });

  it('subscriber receives SQL string for prepared UPDATE statements', (t) => {
    let calls = [];
    using db = new DatabaseSync(':memory:');

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    calls = []; // reset after setup

    using stmt = db.prepare('UPDATE t SET x = ? WHERE x = ?');
    stmt.run(2, 1);

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0].sql, 'UPDATE t SET x = 2.0 WHERE x = 1.0');
  });

  it('subscriber receives SQL string for prepared DELETE statements', (t) => {
    let calls = [];
    using db = new DatabaseSync(':memory:');

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    calls = []; // reset after setup

    using stmt = db.prepare('DELETE FROM t WHERE x = ?');
    stmt.run(1);

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0].sql, 'DELETE FROM t WHERE x = 1.0');
  });

  it('no calls received after unsubscribe', (t) => {
    const calls = [];
    using db = new DatabaseSync(':memory:');

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);

    db.exec('CREATE TABLE t (x INTEGER)');
    assert.strictEqual(calls.length, 1);

    dc.unsubscribe('sqlite.db.query', handler);
    db.exec('INSERT INTO t VALUES (1)');
    assert.strictEqual(calls.length, 1); // No new calls after unsubscribe
  });

  it('falls back to source SQL when expansion fails', (t) => {
    let calls = [];
    using db = new DatabaseSync(':memory:', { limits: { length: 1000 } });

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x TEXT)');
    calls = []; // reset after setup

    using stmt = db.prepare('INSERT INTO t VALUES (?)');

    const longValue = 'a'.repeat(977);
    stmt.run(longValue);

    assert.strictEqual(calls.length, 1);
    // Falls back to source SQL with unexpanded '?' placeholder
    assert.strictEqual(calls[0].sql, 'INSERT INTO t VALUES (?)');
  });

  it('database property identifies the correct database', (t) => {
    const calls = [];
    using db1 = new DatabaseSync(':memory:');
    using db2 = new DatabaseSync(':memory:');

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db1.exec('CREATE TABLE t (x INTEGER)');
    db2.exec('CREATE TABLE t (x INTEGER)');

    assert.strictEqual(calls.length, 2);
    assert.strictEqual(calls[0].database, db1);
    assert.strictEqual(calls[1].database, db2);
    assert.notStrictEqual(calls[0].database, calls[1].database);
  });

  it('duration is a number', (t) => {
    const calls = [];
    using db = new DatabaseSync(':memory:');

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(typeof calls[0].duration, 'number');
  });

  it('duration is non-negative', (t) => {
    const calls = [];
    using db = new DatabaseSync(':memory:');

    const handler = (msg) => calls.push(msg);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');

    assert.strictEqual(calls.length, 1);
    assert.ok(calls[0].duration >= 0);
  });

  it('does not publish when an unfinished statement is collected', async (t) => {
    let calls = 0;
    const handler = () => calls++;
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    let collected = false;
    const registry = new FinalizationRegistry(() => { collected = true; });

    (() => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (x INTEGER)');
      for (let i = 0; i < 10; i++) {
        db.exec(`INSERT INTO t VALUES (${i})`);
      }

      const stmt = db.prepare('SELECT x FROM t');
      registry.register(stmt);
      stmt.iterate().next(); // Leave the statement unfinished.
    })();

    calls = 0; // reset after setup
    await gcUntil('unfinished statement is collected', () => collected);

    assert.strictEqual(calls, 0);
  });

  it('subscriber cannot close the database or statement', (t) => {
    using db = new DatabaseSync(':memory:');

    db.exec('CREATE TABLE t (x INTEGER)');
    using stmt = db.prepare('INSERT INTO t VALUES (?)');

    const handler = mustCall(() => {
      assert.throws(() => db.close(), { code: 'ERR_INVALID_STATE' });
      assert.throws(() => stmt.close(), { code: 'ERR_INVALID_STATE' });
      assert.throws(() => stmt[Symbol.dispose](), {
        code: 'ERR_INVALID_STATE',
      });
    });
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    stmt.run(1);

    dc.unsubscribe('sqlite.db.query', handler);
    assert.deepStrictEqual(db.prepare('SELECT x FROM t').all(), [
      { __proto__: null, x: 1 },
    ]);
  });

  // A subscription from inside a SQLite callback must take effect for the
  // nested query it runs, matching the behavior of a build without USDT
  // probes. Real-subscriber notifications stay immediate.
  it('enables tracing from inside a callback for the nested query', (t) => {
    const events = [];
    const onQuery = (msg) => events.push(msg.sql);

    using db = new DatabaseSync(':memory:');
    db.function('start_listening', () => {
      dc.subscribe('sqlite.db.query', onQuery);
      db.prepare('SELECT 42 AS nested').get();
      return 1;
    });
    db.prepare('SELECT start_listening()').get();
    dc.unsubscribe('sqlite.db.query', onQuery);

    assert.ok(events.includes('SELECT 42 AS nested'));
  });

  // The new blocks below simulate a tracer by writing the USDT probe
  // semaphore view, which is only possible on real-semaphore tiers, so they
  // are skipped (not silently passed) everywhere else.
  const skipNoTracerSimulation =
      !canSimulateTracer &&
      'simulating a tracer requires the live USDT probe semaphore view';

  it('executes all SQL paths under tracer interest without JavaScript dispatch',
     { skip: skipNoTracerSimulation },
     (t) => {

       const origSemaphore = dcBinding.probeSemaphore[0];
       try {
         // The tracer attaches before the database opens and no JavaScript
         // subscriber exists during the sweep.
         dcBinding.probeSemaphore[0] = 1;

         using db = new DatabaseSync(':memory:');
         db.exec('CREATE TABLE t(id INTEGER PRIMARY KEY, value TEXT)');
         const session = db.createSession({ table: 't' });
         using insert = db.prepare('INSERT INTO t VALUES (?, ?)');
         using select = db.prepare('SELECT id, value FROM t WHERE id = ?');
         assert.strictEqual(insert.run(1, 'a').changes, 1);
         assert.deepStrictEqual(select.get(1),
                                { __proto__: null, id: 1, value: 'a' });
         assert.deepStrictEqual(select.all(1),
                                [{ __proto__: null, id: 1, value: 'a' }]);
         for (const row of select.iterate(1)) {
           assert.strictEqual(row.id, 1);
         }
         assert.ok(session.changeset().byteLength > 0);
         assert.ok(session.patchset().byteLength > 0);
         const data = db.serialize();
         assert.ok(data.byteLength > 0);
         const target = new DatabaseSync(':memory:');
         target.exec('CREATE TABLE t(id INTEGER PRIMARY KEY, value TEXT)');
         assert.strictEqual(target.applyChangeset(session.changeset()), true);
         target.deserialize(data);
         const store = db.createTagStore();
         assert.strictEqual(store.run`INSERT INTO t VALUES (${2}, ${'b'})`.changes,
                            1);
         assert.deepStrictEqual(db.prepare('SELECT id FROM t ORDER BY id').all(),
                                [{ __proto__: null, id: 1 },
                                 { __proto__: null, id: 2 }]);
         session.close();
         target.close();

         // Tracer interest alone never dispatches into JavaScript: a
         // subscriber that arrives after the sweep sees only its own events.
         const calls = [];
         const handler = (msg) => calls.push(msg);
         dc.subscribe('sqlite.db.query', handler);
         db.exec("INSERT INTO t VALUES (3, 'c')");
         assert.strictEqual(calls.length, 1);
         assert.strictEqual(calls[0].sql, "INSERT INTO t VALUES (3, 'c')");
         dc.unsubscribe('sqlite.db.query', handler);
       } finally {
         dcBinding.probeSemaphore[0] = origSemaphore;
       }
     });

  it('keeps tracing installed for an attached tracer when the last subscriber leaves',
     { skip: skipNoTracerSimulation },
     (t) => {
       const calls = [];
       const handler = (msg) => calls.push(msg);

       const origSemaphore = dcBinding.probeSemaphore[0];
       try {
         // The tracer attaches before any subscriber and before the database
         // opens.
         dcBinding.probeSemaphore[0] = 1;
         using db = new DatabaseSync(':memory:');
         db.exec('CREATE TABLE t (x INTEGER)');
         assert.strictEqual(calls.length, 0);

         dc.subscribe('sqlite.db.query', handler);
         db.exec('INSERT INTO t VALUES (1)');
         assert.strictEqual(calls.length, 1);

         // The last JavaScript subscriber leaves while the tracer remains
         // attached. Tracing must stay installed for the tracer without
         // dispatching into JavaScript.
         dc.unsubscribe('sqlite.db.query', handler);
         db.exec('INSERT INTO t VALUES (2)');
         assert.strictEqual(calls.length, 1);

         // A subscriber that comes back still receives events.
         dc.subscribe('sqlite.db.query', handler);
         db.exec('INSERT INTO t VALUES (3)');
         assert.strictEqual(calls.length, 2);
       } finally {
         dc.unsubscribe('sqlite.db.query', handler);
         dcBinding.probeSemaphore[0] = origSemaphore;
       }
     });

  it('tracing follows tracer attach, detach, and database reopen',
     { skip: skipNoTracerSimulation },
     (t) => {
       const calls = [];
       const handler = (msg) => calls.push(msg);
       dc.subscribe('sqlite.db.query', handler);
       t.after(() => dc.unsubscribe('sqlite.db.query', handler));

       const origSemaphore = dcBinding.probeSemaphore[0];
       try {
         using db = new DatabaseSync(':memory:');
         dcBinding.probeSemaphore[0] = 0;
         db.exec('CREATE TABLE t (x INTEGER)');
         assert.strictEqual(calls.length, 1);

         // Attaching takes effect at the next execution entry.
         dcBinding.probeSemaphore[0] = 1;
         db.exec('INSERT INTO t VALUES (1)');
         assert.strictEqual(calls.length, 2);

         // Detaching stops future tracer-only production but never removes
         // the real subscriber.
         dcBinding.probeSemaphore[0] = 0;
         db.exec('INSERT INTO t VALUES (2)');
         assert.strictEqual(calls.length, 3);

         // Closing destroys the hook with the connection. Reopening under an
         // attached tracer installs it again. A :memory: database starts empty
         // after reopening, so the table is recreated.
         db.close();
         db.open();
         db.exec('CREATE TABLE t (x INTEGER)');
         dcBinding.probeSemaphore[0] = 1;
         db.exec('INSERT INTO t VALUES (3)');
         assert.strictEqual(calls.length, 5);
       } finally {
         dcBinding.probeSemaphore[0] = origSemaphore;
       }
     });

  it('iterator stepping tolerates tracer attach and detach between steps',
     { skip: skipNoTracerSimulation },
     (t) => {
       const calls = [];
       const handler = (msg) => calls.push(msg);
       dc.subscribe('sqlite.db.query', handler);
       t.after(() => dc.unsubscribe('sqlite.db.query', handler));

       const origSemaphore = dcBinding.probeSemaphore[0];
       try {
         using db = new DatabaseSync(':memory:');
         db.exec('CREATE TABLE t (x INTEGER)');
         db.exec('INSERT INTO t VALUES (1), (2), (3)');
         calls.length = 0;

         using stmt = db.prepare('SELECT x FROM t');
         const iterator = stmt.iterate();
         assert.deepStrictEqual(iterator.next().value, { __proto__: null, x: 1 });
         assert.strictEqual(calls.length, 0);

         // Attaching between steps takes effect on the next step without
         // disturbing the iteration.
         dcBinding.probeSemaphore[0] = 1;
         assert.deepStrictEqual(iterator.next().value, { __proto__: null, x: 2 });
         assert.strictEqual(calls.length, 0);

         dcBinding.probeSemaphore[0] = 0;
         assert.deepStrictEqual(iterator.next().value, { __proto__: null, x: 3 });
         assert.strictEqual(calls.length, 0);

         // The statement finishes on the next step, which fires the profile
         // event for the real subscriber.
         assert.strictEqual(iterator.next().done, true);
         assert.strictEqual(calls.length, 1);
       } finally {
         dcBinding.probeSemaphore[0] = origSemaphore;
       }
     });
});
