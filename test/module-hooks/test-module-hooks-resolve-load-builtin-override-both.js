'use strict';

// This tests the interaction between resolve and load hooks for builtins.
const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');
const fixtures = require('../common/fixtures');

// Pick a builtin that's unlikely to be loaded already - like zlib.
assert(!process.moduleLoadList.includes('NativeModule zlib'));

const redirectedURL = fixtures.fileURL('module-hooks/redirected-zlib.js').href;

registerHooks({
  resolve: common.mustCall(function resolve(specifier, context, nextResolve) {
    assert.strictEqual(specifier, 'zlib');
    return {
      url: redirectedURL,
      format: 'module',
      shortCircuit: true,
    };
  }),

  load: common.mustCall(function load(url, context, nextLoad) {
    assert.strictEqual(url, redirectedURL);
    return {
      source: 'export const loadURL = import.meta.url;',
      format: 'module',
      shortCircuit: true,
    };
  }),
});

const zlib = require('zlib');
assert.strictEqual(zlib.loadURL, redirectedURL);
