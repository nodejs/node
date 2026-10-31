'use strict';
// Imports modules that synchronous load hooks provide as CommonJS, while the
// resolve hook passes on the default resolution when there is one.

const { registerHooks } = require('module');
const path = require('path');
const { pathToFileURL } = require('url');

const missingURL = pathToFileURL(path.join(__dirname, 'missing.cjs')).href;
const mutatedURL = pathToFileURL(path.join(__dirname, 'mutated.mjs')).href;
const formatOnlyURL = pathToFileURL(path.join(__dirname, 'format-only.mjs')).href;
const dataURL = 'data:text/javascript,export default "from data URL";';

const names = new Map([
  [pathToFileURL(path.join(__dirname, 'data.json')).href, 'data.json'],
  [pathToFileURL(path.join(__dirname, 'module.mjs')).href, 'module.mjs'],
  [missingURL, 'missing.cjs'],
  [dataURL, 'data URL'],
]);

registerHooks({
  resolve(specifier, context, nextResolve) {
    try {
      return nextResolve(specifier, context);
    } catch (err) {
      if (specifier === './missing.cjs') {
        return { url: missingURL, shortCircuit: true };
      }
      throw err;
    }
  },
  load(url, context, nextLoad) {
    // The two cases below customize the result from the default step. With
    // asynchronous hooks, they still use the re-invented require(), so they don't
    // report require.cache.
    // TODO: load them with the CommonJS loader in that case too.
    if (url === mutatedURL) {
      // Modify the result in place.
      const result = nextLoad(url, context);
      result.format = 'commonjs';
      result.source = 'module.exports = { name: "mutated.mjs" };';
      return result;
    }
    if (url === formatOnlyURL) {
      // Only override the format of the CommonJS source in a .mjs file.
      return { ...nextLoad(url, context), format: 'commonjs' };
    }
    const name = names.get(url);
    if (name !== undefined) {
      return {
        format: 'commonjs',
        source: `module.exports = { name: ${JSON.stringify(name)}, ` +
                'hasRequireCache: require.cache !== undefined };',
        shortCircuit: true,
      };
    }
    return nextLoad(url, context);
  },
});

// Import the modules one by one to keep the output in order.
let imported = Promise.resolve();
const specifiers = [
  './data.json', './module.mjs', './missing.cjs', dataURL, './mutated.mjs', './format-only.mjs',
];
for (const specifier of specifiers) {
  imported = imported
    .then(() => import(specifier))
    .then(({ default: result }) => console.log(JSON.stringify(result)));
}
