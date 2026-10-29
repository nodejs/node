// Flags: --js-defer-import-eval

// Tests that defer import actually evaluates the imported module
// only when properties that it exports are accessed.

import '../common/index.mjs';
import * as assert from 'assert';

globalThis.eval_list = [];

import defer * as deferred from '../fixtures/es-modules/module-deferred-eval.mjs';

assert.strictEqual(globalThis.eval_list.length, 0);

// Attempts to define a property on the deferred module. This should
// trigger its execution, similar to accessing the `foo` property.
assert.throws(() => Object.defineProperty(deferred.prop, 'newProp', { value: 15 }), TypeError);

assert.strictEqual(deferred.foo, 42);

// Check that the module has been evaluated at this point.
assert.partialDeepStrictEqual(['defer-1'], globalThis.eval_list);

// Clean-up
delete globalThis.eval_list;
