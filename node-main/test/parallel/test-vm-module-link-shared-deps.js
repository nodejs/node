// Flags: --experimental-vm-modules

'use strict';

const common = require('../common');
const vm = require('vm');
const assert = require('assert');

// This test verifies that a module can be returned multiple
// times in the linker function in `module.link(linker)`.
// `module.link(linker)` should handle the race condition of
// `module.status` when the linker function is asynchronous.

// Regression of https://github.com/nodejs/node/issues/59480

const sources = {
  './index.js': `
        import foo from "./foo.js";
        import shared from "./shared.js";
        export default {
            foo,
            shared
        };
    `,
  './foo.js': `
        import shared from "./shared.js";
        export default {
            name: "foo"
        };
    `,
  './shared.js': `
        export default {
            name: "shared",
        };
    `,
};

const moduleCache = new Map();

function getModuleInstance(identifier) {
  let module = moduleCache.get(identifier);

  if (!module) {
    module = new vm.SourceTextModule(sources[identifier], {
      identifier,
    });
    moduleCache.set(identifier, module);
  }

  return module;
}

async function esmImport(identifier) {
  const module = getModuleInstance(identifier);
  const requests = [];

  await module.link(async (specifier, referrer) => {
    requests.push([specifier, referrer.identifier]);
    // Use `Promise.resolve` to defer a tick to create a race condition on
    // `module.status` when a module is being imported several times.
    return Promise.resolve(getModuleInstance(specifier));
  });

  await module.evaluate();
  return [module.namespace, requests];
}

async function test() {
  const { 0: mod, 1: requests } = await esmImport('./index.js');
  assert.strictEqual(mod.default.foo.name, 'foo');
  assert.strictEqual(mod.default.shared.name, 'shared');

  // Assert that there is no duplicated requests.
  assert.deepStrictEqual(requests, [
    // [specifier, referrer]
    ['./foo.js', './index.js'],
    ['./shared.js', './index.js'],
    ['./shared.js', './foo.js'],
  ]);
}

test().then(common.mustCall());
