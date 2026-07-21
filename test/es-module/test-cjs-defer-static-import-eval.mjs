// Flags: --js-defer-import-eval

// Test that uses import.defer for a CJS module. It ensures that:
//   1. the module is imported successfully
//   2. it's evaluated synchronously, regardless of the `defer` modifier.

import '../common/index.mjs';
import * as assert from 'assert';

globalThis.eval_list = [];

// Import the CJS module with the `defer` modifier.
// import defer * as imported from '../fixtures/es-modules/package-type-commonjs/index.js';
import defer * as imported from '../fixtures/es-modules/module-cjs-deferred-eval.js';

// Check that the module hasn't been evaluated yet (which is noted
// by adding a property to `globalThis.eval_list`).
assert.strictEqual(globalThis.eval_list.length, 0);

// Check that the exported properties are accessible and have the
// expected values.
assert.equal(imported.default.foo, 42);
assert.equal(imported.default.identifier, 'package-type-commonjs');

// Check that the module has been evaluated at this point.
assert.partialDeepStrictEqual(['defer-1'], globalThis.eval_list);

// Clean-up
delete globalThis.eval_list;
