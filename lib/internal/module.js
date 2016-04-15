'use strict';

exports = module.exports = {
  makeRequireFunction,
  stripBOM,
  addBuiltinLibsToObject
};

exports.requireDepth = 0;

// Invoke with makeRequireFunction.call(module) where |module| is the
// Module object to use as the context for the require() function.
function makeRequireFunction() {
  const Module = this.constructor;
  const self = this;

  function require(path) {
    try {
      exports.requireDepth += 1;
      return self.require(path);
    } finally {
      exports.requireDepth -= 1;
    }
  }

  function resolve(request) {
    return Module._resolveFilename(request, self);
  }

  require.resolve = resolve;

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

exports.builtinLibs = ['assert', 'buffer', 'child_process', 'cluster',
  'crypto', 'dgram', 'dns', 'domain', 'events', 'fs', 'http', 'https', 'net',
  'os', 'path', 'punycode', 'querystring', 'readline', 'repl', 'stream',
  'string_decoder', 'tls', 'tty', 'url', 'util', 'v8', 'vm', 'zlib'];

function addBuiltinLibsToObject(object) {
  // Make built-in modules available directly (loaded lazily).
  exports.builtinLibs.forEach((name) => {
    Object.defineProperty(object, name, {
      get: () => {
        const lib = require(name);
        // This implicitly invokes the setter, so that this getter is only
        // invoked at most once and does not overwrite anything.
        object[name] = lib;
        return lib;
      },
      // Allow the creation of other globals with this name.
      set: (val) => {
        // Deleting the property before re-assigning it disables the
        // getter/setter mechanism.
        delete object[name];
        object[name] = val;
      },
      configurable: true,
      enumerable: false
    });
  });
}
