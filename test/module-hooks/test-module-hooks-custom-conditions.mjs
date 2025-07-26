// This tests that custom conditions can be used in module resolution hooks.
import '../common/index.mjs';
import { registerHooks } from 'node:module';
import assert from 'node:assert';
import { cjs, esm } from '../fixtures/es-modules/custom-condition/load.cjs';

// Without hooks, the default condition is used.
assert.strictEqual(cjs('foo').result, 'default');
assert.strictEqual((await esm('foo')).result, 'default');

// Prepending 'foo' to the conditions array in the resolve hook should
// allow a CJS to be resolved with that condition.
{
  const hooks = registerHooks({
    resolve(specifier, context, nextResolve) {
      assert(Array.isArray(context.conditions));
      context.conditions = ['foo', ...context.conditions];
      return nextResolve(specifier, context);
    },
  });
  assert.strictEqual(cjs('foo/second').result, 'foo');
  assert.strictEqual((await esm('foo/second')).result, 'foo');
  hooks.deregister();
}

// Prepending 'foo-esm' to the conditions array in the resolve hook should
// allow a ESM to be resolved with that condition.
{
  const hooks = registerHooks({
    resolve(specifier, context, nextResolve) {
      assert(Array.isArray(context.conditions));
      context.conditions = ['foo-esm', ...context.conditions];
      return nextResolve(specifier, context);
    },
  });
  assert.strictEqual(cjs('foo/third').result, 'foo-esm');
  assert.strictEqual((await esm('foo/third')).result, 'foo-esm');
  hooks.deregister();
}

// Duplicating the 'foo' condition in the resolve hook should not change the result.
{
  const hooks = registerHooks({
    resolve(specifier, context, nextResolve) {
      assert(Array.isArray(context.conditions));
      context.conditions = ['foo', ...context.conditions, 'foo'];
      return nextResolve(specifier, context);
    },
  });
  assert.strictEqual(cjs('foo/fourth').result, 'foo');
  assert.strictEqual((await esm('foo/fourth')).result, 'foo');
  hooks.deregister();
}
