'use strict';
require('../common');
const assert = require('assert');
const { test } = require('node:test');

// Disable colored output to prevent color codes from breaking assertion
// message comparisons. This should only be an issue when process.stdout
// is a TTY.
if (process.stdout.isTTY) {
  process.env.NODE_DISABLE_COLORS = '1';
}

// Regression tests for https://github.com/nodejs/node/issues/50397.
//
// When `assert.deepStrictEqual` fails solely because the two values have
// different prototypes, the diff used to render as `{} !== {}` (or similar),
// giving the user no clue about why the assertion failed. The assertion
// error formatter now surfaces the prototype mismatch explicitly.

test('deepStrictEqual surfaces anonymous-class prototype mismatch', () => {
  const A = (() => class {})();
  const B = (() => class {})();
  assert.throws(
    () => assert.deepStrictEqual(new A(), new B()),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.match(err.message, /Object prototypes differ:/);
      // The previous "Values have same structure but are not reference-equal"
      // message must no longer be used for prototype-only mismatches because
      // it is misleading: the values do not even have the same prototype.
      assert.doesNotMatch(err.message, /same structure but are not reference-equal/);
      return true;
    }
  );
});

test('deepStrictEqual surfaces named-class prototype mismatch', () => {
  class Foo {}
  class Bar {}
  assert.throws(
    () => assert.deepStrictEqual(new Foo(), new Bar()),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      // Both class names should appear somewhere in the rendered message:
      // either in the existing inspect-based diff (`+ Foo {}` / `- Bar {}`)
      // or in the new "Object prototypes differ:" line. The important
      // guarantee is that the user can identify both prototypes from the
      // error message alone.
      assert.match(err.message, /Foo/);
      assert.match(err.message, /Bar/);
      return true;
    }
  );
});

test('deepStrictEqual surfaces null-prototype mismatch', () => {
  const a = { __proto__: null };
  const b = {};
  assert.throws(
    () => assert.deepStrictEqual(a, b),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.match(err.message, /null prototype/);
      return true;
    }
  );
});

test('deepStrictEqual prototype-mismatch message is helpful for empty objects', () => {
  // This is the most pathological case: both sides inspect identically as
  // `{}`, so without the prototype-mismatch information the diff body alone
  // is useless. The fix must produce an explanatory line.
  const A = (() => class {})();
  const B = (() => class {})();
  let captured;
  try {
    assert.deepStrictEqual(new A(), new B());
  } catch (err) {
    captured = err;
  }
  assert.ok(captured, 'deepStrictEqual should have thrown');
  assert.match(captured.message, /Object prototypes differ:/);
});

test('strictEqual on structurally-equal arrays still uses notIdentical message', () => {
  // Sanity check that the new code path does not regress the existing
  // behavior of `strictEqual([], [])`. That comparison continues to use
  // the "Values have same structure but are not reference-equal" message
  // because the prototypes do match (both are Array.prototype).
  assert.throws(
    () => assert.strictEqual([], []),
    {
      code: 'ERR_ASSERTION',
      message: 'Values have same structure but are not reference-equal:\n\n[]\n',
    }
  );
});

test('deepStrictEqual on structurally-equal values with same prototype still fails clearly', () => {
  // When the prototypes match but the values are not reference-equal, the
  // existing notIdentical fallback should still apply (deepStrictEqual on
  // two equal-shape objects with the same prototype should normally pass;
  // here we use objects whose enumerable properties differ to exercise the
  // ordinary diff path and confirm it is unaffected).
  assert.throws(
    () => assert.deepStrictEqual({ a: 1 }, { a: 2 }),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      // The ordinary diff path must NOT mention prototype differences
      // because the prototypes are identical.
      assert.doesNotMatch(err.message, /Object prototypes differ:/);
      return true;
    }
  );
});

// The narrowed implementation must NOT emit the prototype line when neither
// side has a custom prototype: comparisons between a plain object/array and
// a null-prototype object already inspect differently, so the existing diff
// path is sufficient and the new branch should not fire there. These tests
// confirm the narrowing scope.

test('deepStrictEqual between plain and null-prototype objects uses ordinary diff', () => {
  const plain = { a: 1 };
  const nullProto = Object.assign({ __proto__: null }, { a: 1 });
  // These two values inspect differently (`{ a: 1 }` vs
  // `[Object: null prototype] { a: 1 }`), so they take the ordinary
  // diff path. The narrowed prototype-mismatch branch must not fire,
  // because at least one side has a non-default prototype but the
  // values do not inspect identically - the existing diff already
  // surfaces the difference.
  assert.throws(
    () => assert.deepStrictEqual(plain, nullProto),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.doesNotMatch(err.message, /Object prototypes differ:/);
      return true;
    }
  );
});

test('strictEqual between two distinct anonymous-class instances still uses notIdentical', () => {
  // strictEqual is a reference-equality check, not deep-equality, so the
  // prototype-mismatch branch must not fire even when both values are
  // anonymous-class instances that inspect identically.
  const A = (() => class {})();
  const B = (() => class {})();
  assert.throws(
    () => assert.strictEqual(new A(), new B()),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.doesNotMatch(err.message, /Object prototypes differ:/);
      return true;
    }
  );
});
