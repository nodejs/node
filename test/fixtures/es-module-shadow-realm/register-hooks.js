'use strict';
const { registerHooks } = require('node:module');

registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier.startsWith('file:///') && specifier.includes('42')) {
      return { shortCircuit: true, url: specifier };
    }
    return nextResolve(specifier, context);
  },
  load(url, context, nextLoad) {
    if (url.startsWith('file:///') && url.includes('42')) {
      return { shortCircuit: true, format: 'module', source: 'export default 42' };
    }
    return nextLoad(url, context);
  },
});
