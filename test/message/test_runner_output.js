// Flags: --no-warnings
'use strict';
require('../common');
const assert = require('node:assert');
const test = require('node:test');
const util = require('util');

test('sync pass todo', (t) => {
  t.todo();
});

test('sync pass todo with message', (t) => {
  t.todo('this is a passing todo');
});

test('sync fail todo', (t) => {
  t.todo();
  throw new Error('thrown from sync fail todo');
});

test('sync fail todo with message', (t) => {
  t.todo('this is a failing todo');
  throw new Error('thrown from sync fail todo with message');
});

test('sync skip pass', (t) => {
  t.skip();
});

test('sync skip pass with message', (t) => {
  t.skip('this is skipped');
});

test('sync pass', (t) => {
  t.diagnostic('this test should pass');
});

test('sync throw fail', () => {
  throw new Error('thrown from sync throw fail');
});

test('async skip pass', async (t) => {
  t.skip();
});

test('async pass', async () => {

});

test('async throw fail', async () => {
  throw new Error('thrown from async throw fail');
});

test('async skip fail', async (t) => {
  t.skip();
  throw new Error('thrown from async throw fail');
});

test('async assertion fail', async () => {
  // Make sure the assert module is handled.
  assert.strictEqual(true, false);
});

test('resolve pass', () => {
  return Promise.resolve();
});

test('reject fail', () => {
  return Promise.reject(new Error('rejected from reject fail'));
});

test('unhandled rejection - passes but warns', () => {
  Promise.reject(new Error('rejected from unhandled rejection fail'));
});

test('async unhandled rejection - passes but warns', async () => {
  Promise.reject(new Error('rejected from async unhandled rejection fail'));
});

test('immediate throw - passes but warns', () => {
  setImmediate(() => {
    throw new Error('thrown from immediate throw fail');
  });
});

test('immediate reject - passes but warns', () => {
  setImmediate(() => {
    Promise.reject(new Error('rejected from immediate reject fail'));
  });
});

test('immediate resolve pass', () => {
  return new Promise((resolve) => {
    setImmediate(() => {
      resolve();
    });
  });
});

test('subtest sync throw fail', async (t) => {
  await t.test('+sync throw fail', (t) => {
    t.diagnostic('this subtest should make its parent test fail');
    throw new Error('thrown from subtest sync throw fail');
  });
});

test('sync throw non-error fail', async (t) => {
  throw Symbol('thrown symbol from sync throw non-error fail');
});

test('level 0a', { concurrency: 4 }, async (t) => {
  t.test('level 1a', async (t) => {
    const p1a = new Promise((resolve) => {
      setTimeout(() => {
        resolve();
      }, 1000);
    });

    return p1a;
  });

  t.test('level 1b', async (t) => {
    const p1b = new Promise((resolve) => {
      resolve();
    });

    return p1b;
  });

  t.test('level 1c', async (t) => {
    const p1c = new Promise((resolve) => {
      setTimeout(() => {
        resolve();
      }, 2000);
    });

    return p1c;
  });

  t.test('level 1d', async (t) => {
    const p1c = new Promise((resolve) => {
      setTimeout(() => {
        resolve();
      }, 1500);
    });

    return p1c;
  });

  const p0a = new Promise((resolve) => {
    setTimeout(() => {
      resolve();
    }, 3000);
  });

  return p0a;
});

test('top level', { concurrency: 2 }, async (t) => {
  t.test('+long running', async (t) => {
    return new Promise((resolve, reject) => {
      setTimeout(resolve, 3000).unref();
    });
  });

  t.test('+short running', async (t) => {
    t.test('++short running', async (t) => {});
  });
});

test('invalid subtest - pass but subtest fails', (t) => {
  setImmediate(() => {
    t.test('invalid subtest fail', () => {
      throw new Error('this should not be thrown');
    });
  });
});

test('sync skip option', { skip: true }, (t) => {
  throw new Error('this should not be executed');
});

test('sync skip option with message', { skip: 'this is skipped' }, (t) => {
  throw new Error('this should not be executed');
});

test('sync skip option is false fail', { skip: false }, (t) => {
  throw new Error('this should be executed');
});

// A test with no arguments provided.
test();

// A test with only a named function provided.
test(function functionOnly() {});

// A test with only an anonymous function provided.
test(() => {});

// A test with only a name provided.
test('test with only a name provided');

// A test with an empty string name.
test('');

// A test with only options provided.
test({ skip: true });

// A test with only a name and options provided.
test('test with a name and options provided', { skip: true });

// A test with only options and a function provided.
test({ skip: true }, function functionAndOptions() {});

// A test whose description needs to be escaped.
test('escaped description \\ # \\#\\');

// A test whose skip message needs to be escaped.
test('escaped skip message', { skip: '#skip' });

// A test whose todo message needs to be escaped.
test('escaped todo message', { todo: '#todo' });

// A test with a diagnostic message that needs to be escaped.
test('escaped diagnostic', (t) => {
  t.diagnostic('#diagnostic');
});

test('callback pass', (t, done) => {
  setImmediate(done);
});

test('callback fail', (t, done) => {
  setImmediate(() => {
    done(new Error('callback failure'));
  });
});

test('sync t is this in test', function(t) {
  assert.strictEqual(this, t);
});

test('async t is this in test', async function(t) {
  assert.strictEqual(this, t);
});

test('callback t is this in test', function(t, done) {
  assert.strictEqual(this, t);
  done();
});

test('callback also returns a Promise', async (t, done) => {
  throw new Error('thrown from callback also returns a Promise');
});

test('callback throw', (t, done) => {
  throw new Error('thrown from callback throw');
});

test('callback called twice', (t, done) => {
  done();
  done();
});

test('callback called twice in different ticks', (t, done) => {
  setImmediate(done);
  done();
});

test('callback called twice in future tick', (t, done) => {
  setImmediate(() => {
    done();
    done();
  });
});

test('callback async throw', (t, done) => {
  setImmediate(() => {
    throw new Error('thrown from callback async throw');
  });
});

test('callback async throw after done', (t, done) => {
  setImmediate(() => {
    throw new Error('thrown from callback async throw after done');
  });

  done();
});

test('only is set but not in only mode', { only: true }, async (t) => {
  // All of these subtests should run.
  await t.test('running subtest 1');
  t.runOnly(true);
  await t.test('running subtest 2');
  await t.test('running subtest 3', { only: true });
  t.runOnly(false);
  await t.test('running subtest 4');
});

test('custom inspect symbol fail', () => {
  const obj = {
    [util.inspect.custom]() {
      return 'customized';
    },
    foo: 1
  };

  throw obj;
});

test('custom inspect symbol that throws fail', () => {
  const obj = {
    [util.inspect.custom]() {
      throw new Error('bad-inspect');
    },
    foo: 1
  };

  throw obj;
});

test('subtest sync throw fails', async (t) => {
  await t.test('sync throw fails at first', (t) => {
    throw new Error('thrown from subtest sync throw fails at first');
  });
  await t.test('sync throw fails at second', (t) => {
    throw new Error('thrown from subtest sync throw fails at second');
  });
});
