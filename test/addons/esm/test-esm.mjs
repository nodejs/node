/**
 * This file is supposed to be loaded by `test-import.js` and `test-require.js`
 * to verify that `import('*.node')` is working properly either been loaded with
 * the ESM loader or the CJS loader.
 */

import { buildType } from '../../common/index.mjs';
import assert from 'node:assert';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';

const require = createRequire(import.meta.url);

export async function run() {
  // binding.node
  {
    const bindingPath = require.resolve(`./build/${buildType}/binding.node`);
    // Test with order of import+require
    const { default: binding, 'module.exports': exports } = await import(pathToFileURL(bindingPath));
    assert.strictEqual(binding, exports);
    assert.strictEqual(binding.hello(), 'world');

    const bindingRequire = require(bindingPath);
    assert.strictEqual(binding, bindingRequire);
    assert.strictEqual(binding.hello, bindingRequire.hello);

    // Test multiple loading of the same addon.
    // ESM cache can not be removed.
    delete require.cache[bindingPath];
    const { default: rebinding } = await import(pathToFileURL(bindingPath));
    assert.strictEqual(rebinding.hello(), 'world');
    assert.strictEqual(binding.hello, rebinding.hello);
  }
}
