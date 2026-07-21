// Flags: --js-defer-import-eval

// Test that uses import.defer for a CJS module. It ensures that:
//   1. the module is imported successfully
//   2. it's evaluated synchronously, regardless of the `defer` modifier.

import '../common/index.mjs';
import * as assert from 'assert';

// Import the CJS module with the `defer` modifier.
import defer * as imported from '../fixtures/es-modules/module-cjs-deferred-eval.js';

globalThis.eval_list = [];

// Check that the exported properties are accessible and have the
// expected values.
assert.strictEqual(imported.foo, 42);
assert.strictEqual(imported.identifier, 'package-type-commonjs');

// Check that the module has been evaluated at this point,
// also that it's not evaluated more than once.
assert.deepStrictEqual(['defer-1'], globalThis.eval_list);

// Clean-up
delete globalThis.eval_list;
