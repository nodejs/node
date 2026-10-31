'use strict';
// Imports a .ts file as CommonJS, with a resolve hook that only overrides the
// format, and a load hook that doesn't provide its own source in the way named
// by the first argument. The require.extensions handler for .ts should load it.

const fs = require('fs');
const Module = require('module');
const path = require('path');
const { pathToFileURL } = require('url');

const targetURL = pathToFileURL(path.join(__dirname, 'target.ts')).href;

Module._extensions['.ts'] = (mod, filename) => {
  const source = fs.readFileSync(filename, 'utf8');
  mod._compile(source.replace('original', 'transformed'), filename);
};

const loadHooks = {
  'pass-through': (url, context, nextLoad) => nextLoad(url, context),
  'spread': (url, context, nextLoad) => ({ ...nextLoad(url, context) }),
  'next-twice': (url, context, nextLoad) => {
    const result = nextLoad(url, context);
    nextLoad(url, context);
    return result;
  },
  'nullish-source': (url, context, nextLoad) => {
    if (url === targetURL) {
      return { format: 'commonjs', source: null, shortCircuit: true };
    }
    return nextLoad(url, context);
  },
};

Module.registerHooks({
  resolve(specifier, context, nextResolve) {
    const result = nextResolve(specifier, context);
    if (result.url === targetURL) {
      return { ...result, format: 'commonjs' };
    }
    return result;
  },
  load: loadHooks[process.argv[2]],
});

import(targetURL).then(({ default: result }) => console.log(result));
