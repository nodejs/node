'use strict';

// This tests that load hooks can override the format of builtin modules
// to 'json' format.
const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

// Pick a builtin that's unlikely to be loaded already - like zlib.
assert(!process.moduleLoadList.includes('NativeModule zlib'));

const hook = registerHooks({
  load: common.mustCall(function load(url, context, nextLoad) {
    // Only intercept zlib builtin
    if (url === 'node:zlib') {
      // Return JSON format to override the builtin
      return {
        source: JSON.stringify({ custom_zlib: 'JSON overridden zlib' }),
        format: 'json',
        shortCircuit: true,
      };
    }
    return nextLoad(url, context);
  }, 2), // Called twice: once for 'zlib', once for 'node:zlib'
});

// Test: Load hook overrides builtin format to json
const zlib = require('zlib');
assert.strictEqual(zlib.custom_zlib, 'JSON overridden zlib');
assert.strictEqual(typeof zlib.createGzip, 'undefined'); // Original zlib API should not be available

// Test with node: prefix
const zlib2 = require('node:zlib');
assert.strictEqual(zlib2.custom_zlib, 'JSON overridden zlib');
assert.strictEqual(typeof zlib2.createGzip, 'undefined');

hook.deregister();
