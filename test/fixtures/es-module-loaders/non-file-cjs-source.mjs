// Serves CommonJS sources under a non-file URL scheme, so that the `require`
// calls they contain have a referrer that the CJS resolver cannot handle.

const sources = {
  'custom:entry': 'module.exports = require("./dep");',
  'custom:dep': 'module.exports = "loaded through the hooks";',
  'custom:missing-dep': 'module.exports = require("./no-such-dep");',
};

export function resolve(specifier, context, next) {
  if (specifier in sources) {
    return { shortCircuit: true, url: specifier };
  }
  if (specifier === './dep') {
    return { shortCircuit: true, url: 'custom:dep' };
  }
  return next(specifier, context);
}

export function load(url, context, next) {
  if (url in sources) {
    return { shortCircuit: true, format: 'commonjs', source: sources[url] };
  }
  return next(url, context);
}
