// Flags: --js-defer-import-eval

// Test that uses import.defer for a CJS module. It ensures that:
//   1. the module is imported successfully;
//   2. it's evaluated synchronously, regardless of the `defer` modifier;
//   3. Evaluation of the imported module is deferred
//      until first namespace access.

import '../common/index.mjs';
import * as assert from 'assert';

// Import the CJS module with the `defer` modifier.
import defer * as imported from '../fixtures/es-modules/module-cjs-deferred-eval.js';

// At this point, the deferred module should not yet be evaluated. Initialize
// the `eval_list`, which will be populated only when the module is evaluated
// for the first time, triggered by namespace access below.
globalThis.eval_list = [];

// Additionally check that the exported properties `foo` and `identifier`
// are defined and have their values assigned at this point.
assert.strictEqual(imported.foo, 42);
assert.strictEqual(imported.identifier, 'package-type-commonjs');

// Check that the module has been evaluated at this point,
// also that it's not evaluated more than once.
assert.deepStrictEqual(['defer-1'], globalThis.eval_list);

// Clean-up
delete globalThis.eval_list;
