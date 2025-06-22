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
  // binding-export-default.node
  {
    const bindingPath = require.resolve(`./build/${buildType}/binding-export-default.node`);
    // Test with order of require+import
    const bindingRequire = require(bindingPath);
    const ns = await import(pathToFileURL(bindingPath));
    assert.strictEqual(ns.default, bindingRequire);

    // As same as ESM-import-CJS, the default export is the value of `module.exports`.
    assert.strictEqual(ns.default, ns['module.exports']);
    assert.strictEqual(ns.default.default(), 'hello world');
  }

  // binding-export-primitive.node
  {
    const bindingPath = require.resolve(`./build/${buildType}/binding-export-primitive.node`);
    const ns = await import(pathToFileURL(bindingPath));

    // As same as ESM-import-CJS, the default export is the value of `module.exports`.
    assert.strictEqual(ns.default, ns['module.exports']);
    assert.strictEqual(ns.default, 'hello world');
  }
}
