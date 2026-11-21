// Flags: --experimental-vm-modules

'use strict';

const common = require('../common');
const vm = require('vm');
const assert = require('assert');

// This test verifies that `module.link(linker)` does not link a dependency
// again when it has already finished linking before another module
// requests it.

// Regression of https://github.com/nodejs/node/issues/66938

const sources = {
  './index.js': `
        import foo from "./foo.js";
        import shared from "./shared.js";
        export default { foo, shared };
    `,
  './foo.js': `
        import shared from "./shared.js";
        export default "foo";
    `,
  './shared.js': `
        import leaf from "./leaf.js";
        export default leaf;
    `,
  './leaf.js': `
        export default "leaf";
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

async function test() {
  const module = getModuleInstance('./index.js');
  const requests = [];

  await module.link(async (specifier, referrer) => {
    requests.push([specifier, referrer.identifier]);
    // Delay `./foo.js` so that `./shared.js` finishes linking before
    // `./foo.js` requests it.
    if (specifier === './foo.js') {
      await new Promise((resolve) => setImmediate(resolve));
    }
    return getModuleInstance(specifier);
  });

  await module.evaluate();
  assert.strictEqual(module.namespace.default.foo, 'foo');
  assert.strictEqual(module.namespace.default.shared, 'leaf');

  // Assert that `./shared.js` is not linked again for `./foo.js`.
  assert.deepStrictEqual(requests, [
    // [specifier, referrer]
    ['./foo.js', './index.js'],
    ['./shared.js', './index.js'],
    ['./leaf.js', './shared.js'],
    ['./shared.js', './foo.js'],
  ]);
}

test().then(common.mustCall());
