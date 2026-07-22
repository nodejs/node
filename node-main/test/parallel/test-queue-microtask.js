'use strict';

const common = require('../common');
const assert = require('assert');

assert.strictEqual(typeof queueMicrotask, 'function');

[
  undefined,
  null,
  0,
  'x = 5',
].forEach((t) => {
  assert.throws(common.mustCall(() => {
    queueMicrotask(t);
  }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

{
  let called = false;
  queueMicrotask(common.mustCall(() => {
    called = true;
  }));
  assert.strictEqual(called, false);
}

queueMicrotask(common.mustCall(function() {
  assert.strictEqual(arguments.length, 0);
}), 'x', 'y');

{
  const q = [];
  Promise.resolve().then(() => q.push('a'));
  queueMicrotask(common.mustCall(() => q.push('b')));
  Promise.reject().catch(() => q.push('c'));

  queueMicrotask(common.mustCall(() => {
    assert.deepStrictEqual(q, ['a', 'b', 'c']);
  }));
}

const eq = [];
process.on('uncaughtException', (e) => {
  eq.push(e);
});

process.on('exit', () => {
  assert.strictEqual(eq.length, 2);
  assert.strictEqual(eq[0].message, 'E1');
  assert.strictEqual(
    eq[1].message, 'Class constructor  cannot be invoked without \'new\'');
});

queueMicrotask(common.mustCall(() => {
  throw new Error('E1');
}));

queueMicrotask(class {});
