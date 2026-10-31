'use strict';
// Test that imported CJS resolved to a non-file URL, or to a file that is not on
// disk, by asynchronous hooks from module.register(), with the source provided by
// synchronous hooks from module.registerHooks(), won't be resolved again by the
// CJS loader (in which case it would throw because the module is not on disk).

const common = require('../common');
const assert = require('assert');
const path = require('path');
const { register, registerHooks } = require('module');
const { pathToFileURL } = require('url');

const missingURL = pathToFileURL(path.join(__dirname, 'missing.cjs')).href;
const sources = new Map([
  ['virtual:entry', 'module.exports = "from virtual URL";'],
  [missingURL, 'module.exports = "from missing file";'],
]);

const asyncHooks = `
export async function resolve(specifier, context, nextResolve) {
  if (specifier === 'virtual:entry') {
    return { url: specifier, format: 'commonjs', shortCircuit: true };
  }
  if (specifier === 'missing') {
    return { url: ${JSON.stringify(missingURL)}, format: 'commonjs', shortCircuit: true };
  }
  return nextResolve(specifier, context);
}
`;
register(`data:text/javascript,${encodeURIComponent(asyncHooks)}`);

const hook = registerHooks({
  resolve: common.mustCall((specifier, context, nextResolve) => {
    return nextResolve(specifier, context);
  }, 2),
  load: common.mustCall((url, context, nextLoad) => {
    if (sources.has(url)) {
      return {
        format: 'commonjs',
        source: sources.get(url),
        shortCircuit: true,
      };
    }
    return nextLoad(url, context);
  }, 2),
});

Promise.all([
  import('virtual:entry'),
  import('missing'),
]).then(common.mustCall(([virtual, missing]) => {
  assert.strictEqual(virtual.default, 'from virtual URL');
  assert.strictEqual(missing.default, 'from missing file');
  hook.deregister();
}));
