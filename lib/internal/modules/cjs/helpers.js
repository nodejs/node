'use strict';

const { validateString } = require('internal/validators');
const { getOptionValue } = require('internal/options');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { isAbsolute } = require('path');

// Invoke with makeRequireFunction(module) where |module| is the Module object
// to use as the context for the require() function.
function makeRequireFunction(mod) {
  const Module = mod.constructor;

  function require(path) {
    return mod.require(path);
  }

  function resolve(request, options) {
    validateString(request, 'request');
    if (options !== undefined && options.paths !== undefined) {
      if (!Array.isArray(options.paths)) {
        throw new ERR_INVALID_ARG_TYPE('options.paths', 'Array', options.paths);
      }
      if (options.paths.length === 0) {
        throw new ERR_OUT_OF_RANGE('options.paths.length', '> 0',
                                   options.paths.length);
      }
      if (!isAbsolute(request) &&
          (request.charAt(0) !== '.' ||
           (request.length > 1 &&
            request.charAt(1) !== '.' &&
            request.charAt(1) !== '/' &&
            (process.platform !== 'win32' || request.charAt(1) !== '\\')))) {
        const paths = new Set();
        options.paths.forEach((path) =>
          Module._nodeModulePaths(path).forEach((modPath) =>
            paths.add(modPath)));
        return Module._resolveFilename(request, mod, false, [...paths]);
      }
    }
    return Module._resolveFilename(request, mod, false);
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
    // that was previously used to replace the shebang line
    content = content.slice(index);
  }
  return content;
}

const builtinLibs = [
  'assert', 'async_hooks', 'buffer', 'child_process', 'cluster', 'crypto',
  'dgram', 'dns', 'domain', 'events', 'fs', 'http', 'http2', 'https', 'net',
  'os', 'path', 'perf_hooks', 'punycode', 'querystring', 'readline', 'repl',
  'stream', 'string_decoder', 'tls', 'trace_events', 'tty', 'url', 'util',
  'v8', 'vm', 'zlib'
];

if (getOptionValue('--experimental-worker')) {
  builtinLibs.push('worker_threads');
  builtinLibs.sort();
}

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

module.exports = exports = {
  addBuiltinLibsToObject,
  builtinLibs,
  makeRequireFunction,
  requireDepth: 0,
  stripBOM,
  stripShebang
};
