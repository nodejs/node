'use strict';

const { Object } = primordials;

const { validateString } = require('internal/validators');
const path = require('path');
const { pathToFileURL } = require('internal/url');
const { URL } = require('url');

// Invoke with makeRequireFunction(module) where |module| is the Module object
// to use as the context for the require() function.
function makeRequireFunction(mod) {
  const Module = mod.constructor;

  function require(path) {
    return mod.require(path);
  }

  function resolve(request, options) {
    validateString(request, 'request');
    return Module._resolveFilename(request, mod, false, options);
  }

  require.resolve = resolve;

  function paths(request) {
    validateString(request, 'request');
    return Module._resolveLookupPaths(request, mod);
  }

  resolve.paths = paths;

  require.main = process.mainModule;

  // Enable support to add extra extension types.
  require.extensions = Module._extensions;

  require.cache = Module._cache;

  return require;
}

/**
 * Remove byte order marker. This catches EF BB BF (the UTF-8 BOM)
 * because the buffer-to-string conversion in `fs.readFileSync()`
 * translates it to FEFF, the UTF-16 BOM.
 */
function stripBOM(content) {
  if (content.charCodeAt(0) === 0xFEFF) {
    content = content.slice(1);
  }
  return content;
}

/**
 * Find end of shebang line and slice it off
 */
function stripShebang(content) {
  // Remove shebang
  if (content.charAt(0) === '#' && content.charAt(1) === '!') {
    // Find end of shebang line and slice it off
    let index = content.indexOf('\n', 2);
    if (index === -1)
      return '';
    if (content.charAt(index - 1) === '\r')
      index--;
    // Note that this actually includes the newline character(s) in the
    // new output. This duplicates the behavior of the regular expression
    // that was previously used to replace the shebang line.
    content = content.slice(index);
  }
  return content;
}

const builtinLibs = [
  'assert', 'async_hooks', 'buffer', 'child_process', 'cluster', 'crypto',
  'dgram', 'dns', 'domain', 'events', 'fs', 'http', 'http2', 'https', 'net',
  'os', 'path', 'perf_hooks', 'punycode', 'querystring', 'readline', 'repl',
  'stream', 'string_decoder', 'tls', 'trace_events', 'tty', 'url', 'util',
  'v8', 'vm', 'worker_threads', 'zlib'
];

if (typeof internalBinding('inspector').open === 'function') {
  builtinLibs.push('inspector');
  builtinLibs.sort();
}

function addBuiltinLibsToObject(object) {
  // Make built-in modules available directly (loaded lazily).
  builtinLibs.forEach((name) => {
    // Goals of this mechanism are:
    // - Lazy loading of built-in modules
    // - Having all built-in modules available as non-enumerable properties
    // - Allowing the user to re-assign these variables as if there were no
    //   pre-existing globals with the same name.

    const setReal = (val) => {
      // Deleting the property before re-assigning it disables the
      // getter/setter mechanism.
      delete object[name];
      object[name] = val;
    };

    Object.defineProperty(object, name, {
      get: () => {
        const lib = require(name);

        // Disable the current getter/setter and set up a new
        // non-enumerable property.
        delete object[name];
        Object.defineProperty(object, name, {
          get: () => lib,
          set: setReal,
          configurable: true,
          enumerable: false
        });

        return lib;
      },
      set: setReal,
      configurable: true,
      enumerable: false
    });
  });
}

function normalizeReferrerURL(referrer) {
  if (typeof referrer === 'string' && path.isAbsolute(referrer)) {
    return pathToFileURL(referrer).href;
  }
  return new URL(referrer).href;
}

module.exports = {
  addBuiltinLibsToObject,
  builtinLibs,
  makeRequireFunction,
  normalizeReferrerURL,
  stripBOM,
  stripShebang
};
