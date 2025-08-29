// Flags: --expose-internals --js-source-phase-imports
'use strict';
const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');
const { ModuleWrap } = internalBinding('module_wrap');

async function testModuleWrap() {
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

  const moduleRequests = foo.getModuleRequests();
  assert.strictEqual(moduleRequests.length, 1);
  assert.strictEqual(moduleRequests[0].specifier, 'bar');

  foo.link([bar]);
  foo.instantiate();

  assert.strictEqual(await foo.evaluate(-1, false), undefined);
  assert.strictEqual(foo.getNamespace().five, 5);

  // Check that the module requests are the same after linking, instantiate, and evaluation.
  assert.deepStrictEqual(moduleRequests, foo.getModuleRequests());
}

// Verify that linking two module with a same ModuleCacheKey throws an error.
function testLinkMismatch() {
  const foo = new ModuleWrap('foo', undefined, `
    import source BarSource from 'bar';
    import bar from 'bar';
`, 0, 0);
  const bar1 = new ModuleWrap('bar', undefined, 'export const five = 5', 0, 0);
  const bar2 = new ModuleWrap('bar', undefined, 'export const six = 6', 0, 0);

  const moduleRequests = foo.getModuleRequests();
  assert.strictEqual(moduleRequests.length, 2);
  assert.strictEqual(moduleRequests[0].specifier, moduleRequests[1].specifier);
  assert.throws(() => {
    foo.link([bar1, bar2]);
  }, {
    code: 'ERR_MODULE_LINK_MISMATCH',
    // Test that ModuleCacheKey::ToString() is used in the error message.
    message: `Module request 'ModuleCacheKey("bar")' at index 0 must be linked to the same module requested at index 1`
  });
}

(async () => {
  await testModuleWrap();
  testLinkMismatch();
})().then(common.mustCall());
