// Flags: --expose-internals --js-source-phase-imports
'use strict';
const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');
const { ModuleWrap } = internalBinding('module_wrap');

async function testModuleWrap() {
  const unlinked = new ModuleWrap('unlinked', undefined, 'export * from "bar";', 0, 0);
  // `moduleWrap.hasAsyncGraph` is only available after been instantiated.
  assert.throws(() => unlinked.hasAsyncGraph, {
    code: 'ERR_MODULE_NOT_INSTANTIATED',
  });
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

  // `moduleWrap.hasAsyncGraph` is only available after been instantiated.
  assert.throws(() => foo.hasAsyncGraph, {
    code: 'ERR_MODULE_NOT_INSTANTIATED',
  });
  foo.link([bar]);
  foo.instantiate();
  // `moduleWrap.hasAsyncGraph` is accessible after been instantiated.
  assert.strictEqual(bar.hasAsyncGraph, false);
  assert.strictEqual(foo.hasAsyncGraph, false);

  assert.strictEqual(await foo.evaluate(-1, false), undefined);
  assert.strictEqual(foo.getNamespace().five, 5);

  // Check that the module requests are the same after linking, instantiate, and evaluation.
  assert.deepStrictEqual(moduleRequests, foo.getModuleRequests());
}

async function testAsyncGraph() {
  const foo = new ModuleWrap('foo', undefined, 'export * from "bar";', 0, 0);
  const bar = new ModuleWrap('bar', undefined, 'await undefined; export const five = 5', 0, 0);

  const moduleRequests = foo.getModuleRequests();
  assert.strictEqual(moduleRequests.length, 1);
  assert.strictEqual(moduleRequests[0].specifier, 'bar');

  // `moduleWrap.hasAsyncGraph` is only available after been instantiated.
  assert.throws(() => foo.hasAsyncGraph, {
    code: 'ERR_MODULE_NOT_INSTANTIATED',
  });
  foo.link([bar]);
  foo.instantiate();
  // `moduleWrap.hasAsyncGraph` is accessible after been instantiated.
  assert.strictEqual(bar.hasAsyncGraph, true);
  assert.strictEqual(foo.hasAsyncGraph, true);

  const evalPromise = foo.evaluate(-1, false);
  assert.throws(() => foo.getNamespace().five, ReferenceError);
  assert.strictEqual(await evalPromise, undefined);
  assert.strictEqual(foo.getNamespace().five, 5);
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
    message: `Module request 'ModuleCacheKey("bar")' at index 1 must be linked to the same module requested at index 0`
  });
}

(async () => {
  await testModuleWrap();
  await testAsyncGraph();
  testLinkMismatch();
})().then(common.mustCall());
