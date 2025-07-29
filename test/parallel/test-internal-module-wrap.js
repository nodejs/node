// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');
const { ModuleWrap } = internalBinding('module_wrap');

const unlinked = new ModuleWrap('unlinked', undefined, 'export * from "bar";', 0, 0);
assert.throws(() => {
  unlinked.instantiate();
}, {
  code: 'ERR_VM_MODULE_LINK_FAILURE',
});

const dependsOnUnlinked = new ModuleWrap('dependsOnUnlinked', undefined, 'export * from "unlinked";', 0, 0);
dependsOnUnlinked.link([unlinked]);
assert.throws(() => {
  dependsOnUnlinked.instantiate();
}, {
  code: 'ERR_VM_MODULE_LINK_FAILURE',
});

const foo = new ModuleWrap('foo', undefined, 'export * from "bar";', 0, 0);
const bar = new ModuleWrap('bar', undefined, 'export const five = 5', 0, 0);

(async () => {
  const moduleRequests = foo.getModuleRequests();
  assert.strictEqual(moduleRequests.length, 1);
  assert.strictEqual(moduleRequests[0].specifier, 'bar');

  foo.link([bar]);
  foo.instantiate();

  assert.strictEqual(await foo.evaluate(-1, false), undefined);
  assert.strictEqual(foo.getNamespace().five, 5);

  // Check that the module requests are the same after linking, instantiate, and evaluation.
  assert.deepStrictEqual(moduleRequests, foo.getModuleRequests());

})().then(common.mustCall());
