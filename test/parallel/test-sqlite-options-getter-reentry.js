'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { backup, DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');

tmpdir.refresh();

const invalidState = {
  code: 'ERR_INVALID_STATE',
  message: /database is not open/,
};

// A property getter on the options bag runs arbitrary JavaScript in the middle
// of the call, so state validated before the options were read can be stale by
// the time it is used.
suite('closing the database from an options getter', () => {
  test('prepare() throws instead of using a closed connection', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.prepare('SELECT 1', {
        get returnArrays() {
          db.close();
          return false;
        },
      });
    }, invalidState);
  });

  test('function() throws instead of using a closed connection', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.function('fn', {
        get useBigIntArguments() {
          db.close();
          return false;
        },
      }, () => 1);
    }, invalidState);
  });

  test('function() throws when the length getter closes the database', (t) => {
    const db = new DatabaseSync(':memory:');
    const fn = () => 1;
    Object.defineProperty(fn, 'length', {
      configurable: true,
      get() {
        db.close();
        return 0;
      },
    });
    t.assert.throws(() => {
      db.function('fn', fn);
    }, invalidState);
  });

  test('aggregate() throws instead of using a closed connection', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.aggregate('agg', {
        get start() {
          db.close();
          return 0;
        },
        step: (acc, value) => acc,
      });
    }, invalidState);
  });

  test('aggregate() throws when the length getter closes the database', (t) => {
    const db = new DatabaseSync(':memory:');
    const step = (acc, value) => acc;
    Object.defineProperty(step, 'length', {
      configurable: true,
      get() {
        db.close();
        return 2;
      },
    });
    t.assert.throws(() => {
      db.aggregate('agg', { start: 0, step });
    }, invalidState);
  });

  test('deserialize() throws instead of using a closed connection', (t) => {
    const source = new DatabaseSync(':memory:');
    source.exec('CREATE TABLE data(value TEXT)');
    const image = source.serialize();

    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.deserialize(image, {
        get dbName() {
          db.close();
          return 'main';
        },
      });
    }, invalidState);
  });

  test('createSession() throws instead of using a closed connection', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.createSession({
        get db() {
          db.close();
          return 'main';
        },
      });
    }, invalidState);
  });

  test('applyChangeset() throws instead of using a closed connection', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY)');
    const session = db.createSession();
    db.exec('INSERT INTO data (key) VALUES (1)');
    const changeset = session.changeset();

    t.assert.throws(() => {
      db.applyChangeset(changeset, {
        get onConflict() {
          db.close();
          return undefined;
        },
      });
    }, invalidState);
  });

  test('backup() throws instead of using a closed connection', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      backup(db, join(tmpdir.path, 'getter-backup.db'), {
        get rate() {
          db.close();
          return 1;
        },
      });
    }, invalidState);
  });
});

// The state check runs before the options bag is read, so a call that is
// already doomed must not execute any of the caller's getters.
test('options getters do not run on an already-closed database', (t) => {
  const source = new DatabaseSync(':memory:');
  source.exec('CREATE TABLE data(key INTEGER PRIMARY KEY)');
  const image = source.serialize();
  const session = source.createSession();
  source.exec('INSERT INTO data (key) VALUES (1)');
  const changeset = session.changeset();

  const cases = {
    prepare: (db, options) => db.prepare('SELECT 1', options),
    function: (db, options) => db.function('fn', options, () => 1),
    aggregate: (db, options) => db.aggregate('agg', options),
    deserialize: (db, options) => db.deserialize(image, options),
    createSession: (db, options) => db.createSession(options),
    applyChangeset: (db, options) => db.applyChangeset(changeset, options),
    backup: (db, options) =>
      backup(db, join(tmpdir.path, 'closed-backup.db'), options),
  };

  // The property each method reads first, and a valid value for it, so that a
  // getter which does run leaves the "did not run" assertion as the failure
  // rather than a type error from the returned value.
  const probes = {
    prepare: ['returnArrays', false],
    function: ['useBigIntArguments', false],
    aggregate: ['start', 0],
    deserialize: ['dbName', 'main'],
    createSession: ['db', 'main'],
    applyChangeset: ['onConflict', undefined],
    backup: ['rate', 1],
  };

  for (const [name, invoke] of Object.entries(cases)) {
    const db = new DatabaseSync(':memory:');
    db.close();

    const [key, value] = probes[name];
    let ran = false;
    const options = {
      get [key]() {
        ran = true;
        return value;
      },
    };

    t.assert.throws(() => invoke(db, options), invalidState, name);
    t.assert.strictEqual(ran, false, `${name} ran an options getter`);
  }
});

suite('resizing a deserialize() buffer from an options getter', () => {
  test('throws rather than handing uninitialized memory to SQLite', (t) => {
    const source = new DatabaseSync(':memory:');
    source.exec('CREATE TABLE data(value TEXT)');
    source.prepare('INSERT INTO data (value) VALUES (?)').run('hello');
    const image = source.serialize();

    const buffer = new ArrayBuffer(image.byteLength, {
      maxByteLength: image.byteLength,
    });
    new Uint8Array(buffer).set(image);

    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.deserialize(new Uint8Array(buffer), {
        get dbName() {
          buffer.resize(1024);
          return 'main';
        },
      });
    }, {
      code: 'ERR_INVALID_STATE',
      message: /"buffer" argument was resized/,
    });
  });

  test('throws when the buffer is detached', (t) => {
    const source = new DatabaseSync(':memory:');
    source.exec('CREATE TABLE data(value TEXT)');
    const image = source.serialize();

    const buffer = new ArrayBuffer(image.byteLength);
    new Uint8Array(buffer).set(image);

    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.deserialize(new Uint8Array(buffer), {
        get dbName() {
          structuredClone(buffer, { transfer: [buffer] });
          return 'main';
        },
      });
    }, {
      code: 'ERR_INVALID_STATE',
      message: /"buffer" argument was resized/,
    });
  });
});

// fn.length is configurable, so it must be validated rather than cast blindly.
suite('non-integer callback length', () => {
  const badLengths = ['abc', {}, [], null, undefined, NaN, 1.5, Symbol.iterator,
                      10n, true, 2 ** 40];

  test('function() rejects a non-integer length', (t) => {
    const db = new DatabaseSync(':memory:');
    for (const value of badLengths) {
      const fn = () => 1;
      Object.defineProperty(fn, 'length', { configurable: true, value });
      t.assert.throws(() => {
        db.function('fn', fn);
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "function\.length" property must be an integer/,
      }, `length=${String(value)}`);
    }
  });

  test('aggregate() rejects a non-integer step length', (t) => {
    const db = new DatabaseSync(':memory:');
    for (const value of badLengths) {
      const step = (acc, next) => acc;
      Object.defineProperty(step, 'length', { configurable: true, value });
      t.assert.throws(() => {
        db.aggregate('agg', { start: 0, step, result: (acc) => acc });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.step\.length" property must be an integer/,
      }, `length=${String(value)}`);
    }
  });

  test('aggregate() rejects a non-integer inverse length', (t) => {
    const db = new DatabaseSync(':memory:');
    for (const value of badLengths) {
      const inverse = (acc, next) => acc;
      Object.defineProperty(inverse, 'length', { configurable: true, value });
      t.assert.throws(() => {
        db.aggregate('agg', {
          start: 0,
          step: (acc, next) => acc,
          inverse,
          result: (acc) => acc,
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.inverse\.length" property must be an integer/,
      }, `length=${String(value)}`);
    }
  });

  test('a normal function length is still accepted', (t) => {
    const db = new DatabaseSync(':memory:');
    db.function('plus', (a, b) => a + b);
    t.assert.strictEqual(db.prepare('SELECT plus(1, 2) AS v').get().v, 3);
  });
});
