// Check various special values of `conditions` in the context object
// when using synchronous module hooks to override the loaders in a
// CJS module.
'use strict';
const common = require('../common');
const { registerHooks } = require('node:module');
const assert = require('node:assert');
const { cjs, esm } = require('../fixtures/es-modules/custom-condition/load.cjs');

(async () => {
  // Setting it to undefined would lead to the default conditions being used.
  {
    const hooks = registerHooks({
      resolve(specifier, context, nextResolve) {
        context.conditions = undefined;
        return nextResolve(specifier, context);
      },
    });
    assert.strictEqual(cjs('foo').result, 'default');
    assert.strictEqual((await esm('foo')).result, 'default');
    hooks.deregister();
  }

  // Setting it to an empty array would lead to the default conditions being used.
  {
    const hooks = registerHooks({
      resolve(specifier, context, nextResolve) {
        context.conditions = [];
        return nextResolve(specifier, context);
      },
    });
    assert.strictEqual(cjs('foo/second').result, 'default');
    assert.strictEqual((await esm('foo/second')).result, 'default');
    hooks.deregister();
  }

  // If the exports have no default export, it should error.
  {
    const hooks = registerHooks({
      resolve(specifier, context, nextResolve) {
        context.conditions = [];
        return nextResolve(specifier, context);
      },
    });
    assert.throws(() => cjs('foo/no-default'), {
      code: 'ERR_PACKAGE_PATH_NOT_EXPORTED',
    });
    await assert.rejects(esm('foo/no-default'), {
      code: 'ERR_PACKAGE_PATH_NOT_EXPORTED',
    });
    hooks.deregister();
  }

  // If the exports have no default export, it should error.
  {
    const hooks = registerHooks({
      resolve(specifier, context, nextResolve) {
        context.conditions = 'invalid';
        return nextResolve(specifier, context);
      },
    });
    assert.throws(() => cjs('foo/third'), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
    await assert.rejects(esm('foo/third'), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
    hooks.deregister();
  }
})().then(common.mustCall());
