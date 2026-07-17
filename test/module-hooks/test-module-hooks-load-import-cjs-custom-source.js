'use strict';
// Test that imported CJS loaded from sync hooks are handled correctly and
// won't invoke the CJS loading steps again (in which case it would throw
// because the source is not on disk).

const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

const virtualModules = new Map([
  ['virtual:shared', 'module.exports.greet = (name) => "hello " + name;'],
  ['virtual:entry', 'const { greet } = require("virtual:shared");\nmodule.exports = greet("world");'],
]);

const hook = registerHooks({
  resolve: common.mustCall((specifier, context, nextResolve) => {
    if (virtualModules.has(specifier)) {
      return {
        url: specifier,
        format: 'commonjs',
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  }, 2),
  load: common.mustCall((url, context, nextLoad) => {
    if (virtualModules.has(url)) {
      return {
        format: 'commonjs',
        source: virtualModules.get(url),
        shortCircuit: true,
      };
    }
    return nextLoad(url, context);
  }, 2),
});

import('virtual:entry').then(common.mustCall((ns) => {
  assert.strictEqual(ns.default, 'hello world');
  hook.deregister();
}));
