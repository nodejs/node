// Flags: --js-defer-import-eval --expose-internals

// Test that uses import.defer for a builtin module. Currently
// defer importing of a synthetic module should be a no-op
// in Node.js as they are born pre-evaluated, so the test
// is mostly a smoke test that Node doesn't crash.

import '../common/index.mjs';
import * as assert from 'assert';

// Check that there are no modules with 'http' in their name loaded yet.
let modules = process.moduleLoadList.filter((item) => item.endsWith('http'));
assert.strictEqual(modules.length, 0);

const intermediate = await import('./import-builtin-module-intermediate.mjs');

// Check that after dynamically importing the module with imports 'http'
// itself, the builtin module is already present in the module list.
modules = process.moduleLoadList.filter((item) => item.endsWith('http'));
assert.partialDeepStrictEqual(modules, ['NativeModule http']);

// Check that the imported module contains some known properties.
assert.notStrictEqual(intermediate.http.STATUS_CODES, undefined);
assert.notStrictEqual(intermediate.http.createServer, undefined);
assert.strictEqual(typeof intermediate.http.createServer, 'function');
