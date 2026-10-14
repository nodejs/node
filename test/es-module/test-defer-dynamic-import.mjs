// Flags: --js-defer-import-eval

// Tests that defer import of a dynamically imported module
// defers the execution of the module until any of its properties
// is accessed for the first time.

import * as common from '../common/index.mjs';
import * as assert from 'assert';

globalThis.eval_list ||= [];

console.log('main module loading...');

await import.defer('../fixtures/es-modules/module-deferred-eval.mjs').then((module) => {
  // Check that the module has been evaluated exactly once.
  assert.strictEqual(globalThis.eval_list.length, 1);
  assert.strictEqual(module.foo, 42);
  assert.partialDeepStrictEqual(['defer-1'], globalThis.eval_list);

  // Clean-up
  delete globalThis.eval_list;
}).then(common.mustCall());
