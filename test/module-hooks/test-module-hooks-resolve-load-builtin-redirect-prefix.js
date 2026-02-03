'use strict';

// This tests the interaction between resolve and load hooks for builtins with the
// `node:` prefix.
const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

// Pick a builtin that's unlikely to be loaded already - like zlib or dns.
assert(!process.moduleLoadList.includes('NativeModule zlib'));
assert(!process.moduleLoadList.includes('NativeModule dns'));

registerHooks({
  resolve: common.mustCall(function resolve(specifier, context, nextResolve) {
    assert.strictEqual(specifier, 'node:dns');
    return {
      url: 'node:zlib',
      shortCircuit: true,
    };
  }),

  load: common.mustCall(function load(url, context, nextLoad) {
    assert.strictEqual(url, 'node:zlib');
    return nextLoad(url, context);
  }),
});

const zlib = require('node:dns');
assert.strictEqual(typeof zlib.createGzip, 'function');
