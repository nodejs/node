// Flags: --js-defer-import-eval

// Tests that defer import of a module that is also eagerly imported
// from another dependency gets executed synchronously, and gets
// executed exactly once.

import '../common/index.mjs';
import * as assert from 'assert';

globalThis.eval_list ||= [];

import * as non_deferred from '../fixtures/es-modules/module-with-module-tree.mjs';
import defer * as deferred from '../fixtures/es-modules/module-deferred-eval.mjs';

// Check that the module has been evaluated exactly once.
assert.strictEqual(globalThis.eval_list.length, 1);
assert.strictEqual(deferred.foo, 42);
assert.partialDeepStrictEqual(['defer-1'], globalThis.eval_list);

// Skip linter warning
assert.strictEqual(non_deferred.bar, 64);

// Clean-up
delete globalThis.eval_list;
