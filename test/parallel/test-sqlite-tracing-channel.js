'use strict';

const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();

const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const dc = require('node:diagnostics_channel');
const { Database } = require('node:sqlite');
const { suite, it } = require('node:test');

const queryChannel = dc.tracingChannel('sqlite.query');

function record(t) {
  const events = [];
  const handlers = {};
  for (const name of ['start', 'end', 'error']) {
    handlers[name] = (context) => events.push({ name, context });
  }
  queryChannel.subscribe(handlers);
  t.after(() => queryChannel.unsubscribe(handlers));
  return events;
}

function names(events) {
  return events.map((event) => event.name);
}

suite('sqlite.query tracing channel', () => {
  it('traces statement run() with source SQL and parameters', (t) => {
    using db = new Database(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    using stmt = db.prepare('INSERT INTO t VALUES (?)');
    const events = record(t);

    const result = stmt.run(42);

    assert.deepStrictEqual(names(events), ['start', 'end']);
    const { context } = events[0];
    assert.strictEqual(context, events[1].context);
    assert.strictEqual(context.sql, 'INSERT INTO t VALUES (?)');
    assert.deepStrictEqual(context.parameters, [42]);
    assert.strictEqual(context.method, 'run');
    assert.strictEqual(context.database, db);
    assert.strictEqual(context.statement, stmt);
    assert.strictEqual(context.result, result);
  });

  it('traces statement get() and all() with named parameters', (t) => {
    using db = new Database(':memory:');
    db.exec('CREATE TABLE t (x INTEGER); INSERT INTO t VALUES (1), (2)');
    using stmt = db.prepare('SELECT x FROM t WHERE x >= $min ORDER BY x');
    const events = record(t);

    stmt.get({ $min: 2 });
    stmt.all({ $min: 1 });

    assert.deepStrictEqual(names(events), ['start', 'end', 'start', 'end']);
    assert.strictEqual(events[0].context.method, 'get');
    assert.deepStrictEqual(events[0].context.parameters, [{ $min: 2 }]);
    assert.deepStrictEqual(events[0].context.result, { __proto__: null, x: 2 });
    assert.strictEqual(events[2].context.method, 'all');
    assert.strictEqual(events[2].context.result.length, 2);
  });

  it('traces exec()', (t) => {
    using db = new Database(':memory:');
    const events = record(t);

    db.exec('CREATE TABLE t (x INTEGER)');

    assert.deepStrictEqual(names(events), ['start', 'end']);
    const { context } = events[0];
    assert.strictEqual(context.sql, 'CREATE TABLE t (x INTEGER)');
    assert.deepStrictEqual(context.parameters, []);
    assert.strictEqual(context.method, 'exec');
    assert.strictEqual(context.database, db);
    assert.strictEqual(context.statement, undefined);
  });

  it('publishes error for SQL that fails to prepare', (t) => {
    using db = new Database(':memory:');
    const events = record(t);

    assert.throws(() => db.exec('SELECT * FROM missing'), {
      code: 'ERR_SQLITE_ERROR',
    });

    assert.deepStrictEqual(names(events), ['start', 'error', 'end']);
    assert.strictEqual(events[1].context.error.code, 'ERR_SQLITE_ERROR');
    assert.match(events[1].context.error.message, /no such table/);
  });

  it('publishes error for a statement that fails while running', (t) => {
    using db = new Database(':memory:');
    db.exec('CREATE TABLE t (x INTEGER PRIMARY KEY)');
    using stmt = db.prepare('INSERT INTO t VALUES (?)');
    stmt.run(1);
    const events = record(t);

    assert.throws(() => stmt.run(1), { errstr: 'constraint failed' });

    assert.deepStrictEqual(names(events), ['start', 'error', 'end']);
    assert.strictEqual(events[1].context.error.errstr, 'constraint failed');
  });

  it('does not trace iterate()', (t) => {
    using db = new Database(':memory:');
    db.exec('CREATE TABLE t (x INTEGER); INSERT INTO t VALUES (1)');
    using stmt = db.prepare('SELECT x FROM t');
    const sql = db.createTagStore();
    const events = record(t);

    assert.strictEqual([...stmt.iterate()].length, 1);
    assert.strictEqual([...sql.iterate`SELECT x FROM t`].length, 1);
    assert.strictEqual(events.length, 0);
  });

  it('traces tag store queries', (t) => {
    using db = new Database(':memory:');
    db.exec('CREATE TABLE t (x INTEGER, y TEXT)');
    const sql = db.createTagStore();
    const events = record(t);

    const info = sql.run`INSERT INTO t VALUES (${1}, ${'a'})`;
    const rows = sql.all`SELECT y FROM t WHERE x = ${1}`;

    assert.deepStrictEqual(names(events), ['start', 'end', 'start', 'end']);
    const { context } = events[0];
    assert.strictEqual(context.sql, 'INSERT INTO t VALUES (?, ?)');
    assert.deepStrictEqual(context.parameters, [1, 'a']);
    assert.strictEqual(context.method, 'run');
    assert.strictEqual(context.database, db);
    assert.strictEqual(context.statement, undefined);
    assert.strictEqual(context.result, info);
    assert.strictEqual(events[2].context.sql, 'SELECT y FROM t WHERE x = ?');
    assert.strictEqual(events[2].context.result, rows);
  });

  it('traces tag store calls made without a template literal', (t) => {
    using db = new Database(':memory:');
    const sql = db.createTagStore();
    const events = record(t);

    assert.throws(() => sql.run('SELECT 1'), { code: 'ERR_INVALID_ARG_TYPE' });

    assert.deepStrictEqual(names(events), ['start', 'error', 'end']);
    assert.strictEqual(events[0].context.sql, undefined);
    assert.strictEqual(events[0].context.database, db);
  });

  it('keeps a store bound to start active during the query', (t) => {
    using db = new Database(':memory:');
    const als = new AsyncLocalStorage();
    const seen = [];
    db.function('probe', () => {
      seen.push(als.getStore());
      return 1;
    });
    queryChannel.start.bindStore(als, (context) => context.sql);
    t.after(() => queryChannel.start.unbindStore(als));

    db.prepare('SELECT probe()').get();

    assert.deepStrictEqual(seen, ['SELECT probe()']);
    assert.strictEqual(als.getStore(), undefined);
  });

  it('reports a finalized statement without masking its error', (t) => {
    using db = new Database(':memory:');
    const stmt = db.prepare('SELECT 1');
    stmt.close();
    const events = record(t);

    assert.throws(() => stmt.get(), { code: 'ERR_INVALID_STATE' });

    assert.deepStrictEqual(names(events), ['start', 'error', 'end']);
    assert.strictEqual(events[0].context.sql, undefined);
    assert.strictEqual(events[0].context.statement, stmt);
  });

  it('keeps receiver errors unchanged', (t) => {
    using db = new Database(':memory:');
    const stmt = db.prepare('SELECT 1');
    const { run } = Object.getPrototypeOf(stmt);
    let expected;
    try {
      run.call({});
    } catch (err) {
      expected = err;
    }
    assert.ok(expected);
    const events = record(t);

    assert.throws(() => run.call({}), {
      name: expected.name,
      message: expected.message,
    });
    assert.deepStrictEqual(names(events), ['start', 'error', 'end']);
  });

  it('publishes exactly once for each query method', (t) => {
    using db = new Database(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    using stmt = db.prepare('SELECT x FROM t');
    const store = db.createTagStore();

    // Every method on these prototypes is listed, so that a new method that
    // runs SQL cannot be added without deciding whether it is traced.
    const traced = {
      Database: ['exec'],
      Statement: ['all', 'get', 'run'],
      SQLTagStore: ['all', 'get', 'run'],
    };
    const untraced = {
      Database: [
        'aggregate', 'applyChangeset', 'close', 'constructor', 'createModule',
        'createSession', 'createTagStore', 'deserialize', 'enableDefensive',
        'enableLoadExtension', 'function', 'loadExtension', 'location', 'open',
        'prepare', 'serialize', 'setAuthorizer',
      ],
      Statement: [
        'close', 'columns', 'constructor', 'iterate', 'resetStats',
        'setAllowBareNamedParameters', 'setAllowUnknownNamedParameters',
        'setReadBigInts', 'setReturnArrays', 'stat',
      ],
      SQLTagStore: ['clear', 'constructor', 'iterate'],
    };
    const prototypes = {
      Database: Database.prototype,
      Statement: Object.getPrototypeOf(stmt),
      SQLTagStore: Object.getPrototypeOf(store),
    };
    for (const [name, prototype] of Object.entries(prototypes)) {
      const methods = Object.getOwnPropertyNames(prototype)
        .filter((key) => typeof Object.getOwnPropertyDescriptor(prototype, key)
          .value === 'function')
        .sort();
      assert.deepStrictEqual(
        methods, [...traced[name], ...untraced[name]].sort(), name);
    }

    const calls = {
      Database: { exec: () => db.exec('SELECT 1') },
      Statement: {
        all: () => stmt.all(),
        get: () => stmt.get(),
        run: () => stmt.run(),
      },
      SQLTagStore: {
        all: () => store.all`SELECT 1`,
        get: () => store.get`SELECT 1`,
        run: () => store.run`SELECT 1`,
      },
    };
    const events = record(t);
    for (const [name, methods] of Object.entries(traced)) {
      for (const method of methods) {
        events.length = 0;
        calls[name][method]();
        assert.deepStrictEqual(names(events), ['start', 'end'],
                               `${name}#${method}`);
        assert.strictEqual(events[0].context.method, method);
      }
    }
  });

  it('publishes nothing without subscribers', () => {
    using db = new Database(':memory:');
    assert.strictEqual(queryChannel.hasSubscribers, false);
    db.exec('CREATE TABLE t (x INTEGER)');
    assert.deepStrictEqual(db.prepare('SELECT count(*) AS n FROM t').get(),
                           { __proto__: null, n: 0 });
  });
});
