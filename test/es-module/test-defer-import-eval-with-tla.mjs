// Flags: --js-defer-import-eval

// Tests that defer import of a module with top-level await
// eagerly evaluates the imported module.

import '../common/index.mjs';
import * as assert from 'assert';

// The importing module will only execute once its dependency containing
// top-level await has its promises resolved, as per
// https://github.com/tc39/proposal-top-level-await#what-exactly-is-blocked-by-a-top-level-await
if (!globalThis.eval_list) {
  globalThis.eval_list = [];
}

import defer * as ns from '../fixtures/es-modules/dep-module-top-level-await.mjs';

// Check that the module has already been evaluated.
assert.strictEqual(globalThis.eval_list.length, 1);
assert.strictEqual(ns.foo, 42);
assert.partialDeepStrictEqual(['defer-tla-1'], globalThis.eval_list);

// Clean-up
delete globalThis.eval_list;
