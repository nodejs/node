import '../common/index.mjs';
import assert from 'node:assert';
import Module from 'node:module';

// This test verifies that importing two modules with different import
// attributes should result in different module instances, if the module loader
// resolves to module instances.
//
// For example,
// ```mjs
//   import * as secret1 from '../primitive-42.json' with { type: 'json' };
//   import * as secret2 from '../primitive-42.json';
// ```
// should result in two different module instances, if the second import
// is been evaluated as a CommonJS module.
//
// ECMA-262 requires that in `HostLoadImportedModule`, if the operation is called
// multiple times with two (referrer, moduleRequest) pairs, it should return the same
// result. But if the moduleRequest is different, it should return a different,
// and the module loader resolves to different module instances, it should return
// different module instances.
// Refs: https://tc39.es/ecma262/#sec-HostLoadImportedModule

const kJsonModuleSpecifier = '../primitive-42.json';
const fixtureUrl = new URL('../fixtures/primitive-42.json', import.meta.url).href;

Module.registerHooks({
  resolve: (specifier, context, nextResolve) => {
    if (kJsonModuleSpecifier !== specifier) {
      return nextResolve(specifier, context);
    }
    if (context.importAttributes.type === 'json') {
      return {
        url: fixtureUrl,
        // Return a new importAttributes object to ensure
        // that the module loader treats this as a same module request.
        importAttributes: {
          type: 'json',
        },
        shortCircuit: true,
        format: 'json',
      };
    }
    return {
      url: fixtureUrl,
      // Return a new importAttributes object to ensure
      // that the module loader treats this as a same module request.
      importAttributes: {},
      shortCircuit: true,
      format: 'module',
    };
  },
});

const { secretModule, secretJson, secretJson2 } = await import('../fixtures/es-modules/json-modules-attributes.mjs');
assert.notStrictEqual(secretModule, secretJson);
assert.strictEqual(secretJson, secretJson2);
assert.strictEqual(secretJson.default, 42);
assert.strictEqual(secretModule.default, undefined);
