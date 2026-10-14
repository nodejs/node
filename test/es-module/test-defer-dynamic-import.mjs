// Flags: --js-defer-import-eval

// Tests that defer import of a dynamically imported module
// defers the execution of the module until any of its properties
// is accessed for the first time.

import * as common from '../common/index.mjs';
import * as assert from 'assert';

globalThis.eval_list = [];

await import.defer('../fixtures/es-modules/module-deferred-eval.mjs').then(
  (module) => {
    // The module evaluation should be deferred until the namespace (that is,
    // any of its exported properties) is accessed.
    assert.deepStrictEqual(globalThis.eval_list, [],
                           new SyntaxError('imported module should have been deferred until first property access'));

    // Accessing the namespace should trigger the evaluation of the module.
    assert.strictEqual(module.foo, 42);
    assert.deepStrictEqual(globalThis.eval_list, ['defer-1'],
                           new Error('imported module should have been executed only once'));

    // Clean-up
    delete globalThis.eval_list;
  }).then(common.mustCall());
