'use strict';

// Verifies that `requireStack` is attached uniformly across the
// module-resolution error paths the CJS loader walks. The historical
// behavior attached `requireStack` only on the bottom fallback path of
// `Module._resolveFilename`; the inner helpers (`tryPackage`, `trySelf`,
// `Module._findPath`, `createEsmNotFoundErr` via the `#imports` branch
// and `finalizeEsmResolution`) all threw without it.

require('../common');
const assert = require('assert');
const Module = require('module');

// 1. `#imports` resolves to a non-existent file. finalizeEsmResolution
//    throws a MODULE_NOT_FOUND error from within the `#imports` branch's
//    inner try/catch. The branch must enrich the error with requireStack
//    before re-throwing, otherwise it escapes via the unrelated-code
//    fall-through path.
{
  const entry = require.resolve('../fixtures/packages/missing-imports');
  assert.throws(
    () => require('../fixtures/packages/missing-imports'),
    (err) => {
      assert.strictEqual(err.code, 'MODULE_NOT_FOUND');
      assert.ok(Array.isArray(err.requireStack),
                `expected array requireStack, got ${err.requireStack}`);
      assert.ok(err.requireStack.includes(entry),
                `requireStack ${JSON.stringify(err.requireStack)} ` +
                `did not contain ${entry}`);
      assert.ok(err.requireStack.includes(__filename),
                `requireStack ${JSON.stringify(err.requireStack)} ` +
                `did not contain ${__filename}`);
      return true;
    },
  );
}

// 2. A self-require of a subpath that is not present in the package's
//    `exports` map produces ERR_PACKAGE_PATH_NOT_EXPORTED from
//    `packageExportsResolve` inside `trySelf`. The new wrapper must
//    attach requireStack for this code as well, not only for
//    MODULE_NOT_FOUND. A relative require would bypass the exports
//    enforcement entirely, so this case uses a self-require to reach
//    `packageExportsResolve`.
{
  const entry = require.resolve('../fixtures/packages/restrictive-self-require');
  assert.throws(
    () => require('../fixtures/packages/restrictive-self-require'),
    (err) => {
      assert.strictEqual(err.code, 'ERR_PACKAGE_PATH_NOT_EXPORTED');
      assert.ok(Array.isArray(err.requireStack),
                `expected array requireStack, got ${err.requireStack}`);
      assert.ok(err.requireStack.includes(entry),
                `requireStack ${JSON.stringify(err.requireStack)} ` +
                `did not contain ${entry}`);
      assert.ok(err.requireStack.includes(__filename),
                `requireStack ${JSON.stringify(err.requireStack)} ` +
                `did not contain ${__filename}`);
      return true;
    },
  );
}

// 3. `restrictive-exports` itself resolves cleanly. This is the happy path
//    that proves the requireStack work does not regress successful
//    resolutions (the eager-build version had a measurable cost here).
{
  const mod = require('../fixtures/packages/restrictive-exports');
  assert.deepStrictEqual(mod, { ok: true });
}

// 4. A monkey-patched `Module._findPath` that throws a frozen
//    MODULE_NOT_FOUND error must propagate cleanly. The loader's attempt
//    to attach requireStack must not throw a TypeError on the
//    non-extensible error and replace the original failure.
{
  const originalFindPath = Module._findPath;
  const frozenMessage = 'frozen module not found from monkey-patch';
  Module._findPath = function() {
    const err = new Error(frozenMessage);
    err.code = 'MODULE_NOT_FOUND';
    Object.freeze(err);
    throw err;
  };

  try {
    assert.throws(
      () => require('this-module-does-not-exist-and-never-will'),
      (err) => {
        assert.strictEqual(err.code, 'MODULE_NOT_FOUND');
        assert.strictEqual(err.message, frozenMessage);
        assert.ok(!(err instanceof TypeError),
                  'loader masked the original error with a TypeError');
        return true;
      },
    );
  } finally {
    Module._findPath = originalFindPath;
  }
}

// 5. A monkey-patched `Module._findPath` that throws an error whose
//    `requireStack` is already set as a non-array sentinel must not be
//    silently overwritten with a stale value, and must not crash the
//    loader. The defensive `ArrayIsArray` check controls this.
{
  const originalFindPath = Module._findPath;
  Module._findPath = function() {
    const err = new Error('preset non-array requireStack');
    err.code = 'MODULE_NOT_FOUND';
    err.requireStack = null;
    throw err;
  };

  try {
    assert.throws(
      () => require('another-module-that-does-not-exist'),
      (err) => {
        assert.strictEqual(err.code, 'MODULE_NOT_FOUND');
        // Loader replaces the non-array sentinel with the actual chain.
        assert.ok(Array.isArray(err.requireStack),
                  `expected array requireStack, got ${err.requireStack}`);
        assert.ok(err.requireStack.includes(__filename));
        return true;
      },
    );
  } finally {
    Module._findPath = originalFindPath;
  }
}
