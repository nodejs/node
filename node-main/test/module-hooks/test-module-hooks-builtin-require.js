'use strict';

// This tests that when builtins that demand the `node:` prefix are
// required, the URL returned by the default resolution step is still
// prefixed and valid, and that gets passed to the load hook is still
// the one with the `node:` prefix. The one with the prefix
// stripped for internal lookups should not get passed into the hooks.

const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

const schemelessBlockList = new Set([
  'sea',
  'sqlite',
  'test',
  'test/reporters',
]);

const testModules = [];
for (const mod of schemelessBlockList) {
  testModules.push(`node:${mod}`);
}

const hook = registerHooks({
  resolve: common.mustCall((specifier, context, nextResolve) => {
    const result = nextResolve(specifier, context);
    assert.match(result.url, /^node:/);
    assert.strictEqual(schemelessBlockList.has(result.url.slice(5, result.url.length)), true);
    return result;
  }, testModules.length),
  load: common.mustCall(function load(url, context, nextLoad) {
    assert.match(url, /^node:/);
    assert.strictEqual(schemelessBlockList.has(url.slice(5, url.length)), true);
    const result = nextLoad(url, context);
    assert.strictEqual(result.source, null);
    return {
      source: 'throw new Error("I should not be thrown because the loader ignores user-supplied source for builtins")',
      format: 'builtin',
    };
  }, testModules.length),
});

for (const mod of testModules) {
  require(mod);
}

hook.deregister();
