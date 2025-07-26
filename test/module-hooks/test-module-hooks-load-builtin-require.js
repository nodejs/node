'use strict';

const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

// This tests that required builtins get null as source from default
// step, and the source returned are ignored.
// TODO(joyeecheung): this is to align with the module.register() behavior
// but perhaps the load hooks should not be invoked for builtins at all.

// Pick a builtin that's unlikely to be loaded already - like zlib.
assert(!process.moduleLoadList.includes('NativeModule zlib'));
let hook;

hook = registerHooks({
  load: common.mustCall(function load(url, context, nextLoad) {
    assert.strictEqual(url, 'node:zlib');
    const result = nextLoad(url, context);
    assert.strictEqual(result.source, null);
    return {
      source: 'throw new Error("I should not be thrown")',
      format: 'builtin',
    };
  }),
});

assert.strictEqual(typeof require('zlib').createGzip, 'function');

hook.deregister();

// This tests that when builtins that demand the `node:` prefix are
// required, the URL returned by the default resolution step is still
// prefixed and valid, and that gets passed to the load hook is still
// the one with the `node:` prefix. The one with the prefix
// stripped for internal lookups should not get passed into the hooks.
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

hook = registerHooks({
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
