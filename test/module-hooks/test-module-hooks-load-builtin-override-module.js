'use strict';

// This tests that load hooks can override the format of builtin modules
// to 'module', and require() can load them.
const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

// Pick a builtin that's unlikely to be loaded already - like zlib.
assert(!process.moduleLoadList.includes('NativeModule zlib'));

const hook = registerHooks({
  load: common.mustCall(function load(url, context, nextLoad) {
    // Only intercept zlib builtin
    if (url === 'node:zlib') {
      // Return ES module format to override the builtin
      // Note: For require() to work with ESM, we need to export 'module.exports'
      return {
        source: `const exports = { custom_zlib: "ESM overridden zlib" };
                 export default exports;
                 export { exports as 'module.exports' };`,
        format: 'module',
        shortCircuit: true,
      };
    }
    return nextLoad(url, context);
  }, 2), // Called twice: once for 'zlib', once for 'node:zlib'
});

// Test: Load hook overrides builtin format to module.
// With the 'module.exports' export, require() should work
const zlib = require('zlib');
assert.strictEqual(zlib.custom_zlib, 'ESM overridden zlib');
assert.strictEqual(typeof zlib.createGzip, 'undefined'); // Original zlib API should not be available

// Test with node: prefix
const zlib2 = require('node:zlib');
assert.strictEqual(zlib2.custom_zlib, 'ESM overridden zlib');
assert.strictEqual(typeof zlib2.createGzip, 'undefined');

hook.deregister();
