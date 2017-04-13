'use strict';

const assert = require('assert');

exports = module.exports = {
  makeRequireFunction,
  stripBOM,
  addBuiltinLibsToObject,
  applyTransforms
};

exports.requireDepth = 0;

const transforms = [];

function addTransform(transform) {
  assert(typeof transform === 'function');
  transforms.push(transform);
}
function removeTransform(transform) {
  const index = transforms.indexOf(transform);
  if (index >= 0) {
    transforms.splice(index, 1);
  }
}

function applyTransforms(content, filename) {
  return transforms.reduce((content, transform) => {
    const next = transform(content, filename);
    if (typeof next !== 'string') {
      throw new Error('Module transforms must return the modified content');
    }
    return next;
  }, content);
}

// Invoke with makeRequireFunction(module) where |module| is the Module object
// to use as the context for the require() function.
function makeRequireFunction(mod) {
  const Module = mod.constructor;

  function require(path) {
    try {
      exports.requireDepth += 1;
      return mod.require(path);
    } finally {
      exports.requireDepth -= 1;
    }
  }

  function resolve(request) {
    return Module._resolveFilename(request, mod);
  }

  require.resolve = resolve;
  require.addTransform = addTransform;
  require.removeTransform = removeTransform;

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

exports.builtinLibs = [
  'assert', 'buffer', 'child_process', 'cluster', 'crypto', 'dgram', 'dns',
  'domain', 'events', 'fs', 'http', 'https', 'net', 'os', 'path', 'punycode',
  'querystring', 'readline', 'repl', 'stream', 'string_decoder', 'tls', 'tty',
  'url', 'util', 'v8', 'vm', 'zlib'
];

function addBuiltinLibsToObject(object) {
  // Make built-in modules available directly (loaded lazily).
  exports.builtinLibs.forEach((name) => {
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
