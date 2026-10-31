'use strict';

// This tests that load hooks can override the format of imported builtin
// modules to 'commonjs' after calling nextLoad().
const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

// Pick a builtin that's unlikely to be loaded already - like zlib.
assert(!process.moduleLoadList.includes('NativeModule zlib'));

const hook = registerHooks({
  load: common.mustCall(function load(url, context, nextLoad) {
    const result = nextLoad(url, context);
    if (url === 'node:zlib' && result.format === 'builtin') {
      return {
        source: 'exports.custom_zlib = "overridden by load hook";',
        format: 'commonjs',
        shortCircuit: true,
      };
    }
    return result;
  }, 2), // Called by the ESM loader, then again by Module._load() for the builtin.
});

import('node:zlib').then(common.mustCall((ns) => {
  assert.strictEqual(ns.default.custom_zlib, 'overridden by load hook');
  assert.strictEqual(ns.default.createGzip, undefined);
  hook.deregister();
}));
