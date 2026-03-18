'use strict';

const common = require('../../common');
const assert = require('assert');
const vm = require('vm');

const binding = require(`./build/${common.buildType}/binding`);

// This verifies that the addon-created context has an independent
// global object.
{
  const ctx = binding.makeContext();
  const result = binding.runInContext(ctx, `
  globalThis.foo = 'bar';
  foo;
  `);
  assert.strictEqual(result, 'bar');
  assert.strictEqual(globalThis.foo, undefined);

  // Verifies that eval can be disabled in the addon-created context.
  assert.throws(() => binding.runInContext(ctx, `
  eval('"foo"');
  `), { name: 'EvalError' });

  // Verifies that the addon-created context does not setup import loader.
  const p = binding.runInContext(ctx, `
  const p = import('node:fs');
  p;
  `);
  p.catch(common.mustCall((e) => {
    assert.throws(() => { throw e; }, { code: 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING' });
  }));
}


// Verifies that the addon can unwrap the context from a vm context
{
  const ctx = vm.createContext(vm.constants.DONT_CONTEXTIFY);
  vm.runInContext('globalThis.foo = {}', ctx);
  const result = binding.runInContext(ctx, 'globalThis.foo');
  // The return value identities should be equal.
  assert.strictEqual(result, vm.runInContext('globalThis.foo', ctx));
}
