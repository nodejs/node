'use strict';
// Tests that import() never evaluates the module before returning to its
// caller, even though loading and linking are synchronous.
const common = require('../common');
const assert = require('node:assert');

const key = '__esmImportDefersEvaluation';
const url = `data:text/javascript,globalThis.${key} = true; export default 1;`;

const promise = import(url);
assert.strictEqual(globalThis[key], undefined);
promise.then(common.mustCall((namespace) => {
  assert.strictEqual(globalThis[key], true);
  assert.strictEqual(namespace.default, 1);
  delete globalThis[key];
}));
