'use strict';

const {
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ObjectDefineProperty,
  ObjectPrototypeHasOwnProperty,
  SafeMap,
  SafeSet,
  StringPrototypeCharCodeAt,
  StringPrototypeIncludes,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;
const {
  ERR_INVALID_ARG_TYPE,
  ERR_MANIFEST_DEPENDENCY_MISSING,
  ERR_UNKNOWN_BUILTIN_MODULE,
} = require('internal/errors').codes;
const { BuiltinModule } = require('internal/bootstrap/realm');

const { validateString } = require('internal/validators');
const fs = require('fs'); // Import all of `fs` so that it can be monkey-patched.
const internalFS = require('internal/fs/utils');
const path = require('path');
const { pathToFileURL, fileURLToPath, URL } = require('internal/url');

const { getOptionValue } = require('internal/options');
const { setOwnProperty } = require('internal/util');

const {
  privateSymbols: {
    require_private_symbol,
  },
} = internalBinding('util');

let debug = require('internal/util/debuglog').debuglog('module', (fn) => {
  debug = fn;
});

/** @typedef {import('internal/modules/cjs/loader.js').Module} Module */

/**
 * Cache for storing resolved real paths of modules.
 * In order to minimize unnecessary lstat() calls, this cache is a list of known-real paths.
 * Set to an empty Map to reset.
 * @type {Map<string, string>}
 */
const realpathCache = new SafeMap();
/**
 * Resolves the path of a given `require` specifier, following symlinks.
 * @param {string} requestPath The `require` specifier
 */
function toRealPath(requestPath) {
  return fs.realpathSync(requestPath, {
    [internalFS.realpathCacheKey]: realpathCache,
  });
}

/** @type {Set<string>} */
let cjsConditions;
/**
 * Define the conditions that apply to the CommonJS loader.
 */
function initializeCjsConditions() {
  const userConditions = getOptionValue('--conditions');
  const noAddons = getOptionValue('--no-addons');
  const addonConditions = noAddons ? [] : ['node-addons'];
  // TODO: Use this set when resolving pkg#exports conditions in loader.js.
  cjsConditions = new SafeSet([
    'require',
    'node',
    ...addonConditions,
    ...userConditions,
  ]);
}

/**
 * Get the conditions that apply to the CommonJS loader.
 */
function getCjsConditions() {
  if (cjsConditions === undefined) {
    initializeCjsConditions();
  }
  return cjsConditions;
}

/**
 * Provide one of Node.js' public modules to user code.
 * @param {string} id - The identifier/specifier of the builtin module to load
 * @param {string} request - The module requiring or importing the builtin module
 */
function loadBuiltinModule(id, request) {
  if (!BuiltinModule.canBeRequiredByUsers(id)) {
    return;
  }
  /** @type {import('internal/bootstrap/realm.js').BuiltinModule} */
  const mod = BuiltinModule.map.get(id);
  debug('load built-in module %s', request);
  // compileForPublicLoader() throws if canBeRequiredByUsers is false:
  mod.compileForPublicLoader();
  return mod;
}

/** @type {Module} */
let $Module = null;
/**
 * Import the Module class on first use.
 */
function lazyModule() {
  $Module = $Module || require('internal/modules/cjs/loader').Module;
  return $Module;
}

/**
 * Invoke with `makeRequireFunction(module)` where `module` is the `Module` object to use as the context for the
 * `require()` function.
 * Use redirects to set up a mapping from a policy and restrict dependencies.
 */
const urlToFileCache = new SafeMap();
/**
 * Create the module-scoped `require` function to pass into CommonJS modules.
 * @param {Module} mod - The module to create the `require` function for.
 * @param {ReturnType<import('internal/policy/manifest.js').Manifest['getDependencyMapper']>} redirects
 * @typedef {(specifier: string) => unknown} RequireFunction
 */
function makeRequireFunction(mod, redirects) {
  // lazy due to cycle
  const Module = lazyModule();
  if (mod instanceof Module !== true) {
    throw new ERR_INVALID_ARG_TYPE('mod', 'Module', mod);
  }

  /** @type {RequireFunction} */
  let require;
  if (redirects) {
    const id = mod.filename || mod.id;
    const conditions = getCjsConditions();
    const { resolve, reaction } = redirects;
    require = function require(specifier) {
      let missing = true;
      const destination = resolve(specifier, conditions);
      if (destination === true) {
        missing = false;
      } else if (destination) {
        const { href, protocol } = destination;
        if (protocol === 'node:') {
          const specifier = destination.pathname;

          if (BuiltinModule.canBeRequiredByUsers(specifier)) {
            const mod = loadBuiltinModule(specifier, href);
            return mod.exports;
          }
          throw new ERR_UNKNOWN_BUILTIN_MODULE(specifier);
        } else if (protocol === 'file:') {
          let filepath = urlToFileCache.get(href);
          if (!filepath) {
            filepath = fileURLToPath(destination);
            urlToFileCache.set(href, filepath);
          }
          return mod[require_private_symbol](mod, filepath);
        }
      }
      if (missing) {
        reaction(new ERR_MANIFEST_DEPENDENCY_MISSING(
          id,
          specifier,
          ArrayPrototypeJoin([...conditions], ', '),
        ));
      }
      return mod[require_private_symbol](mod, specifier);
    };
  } else {
    require = function require(path) {
      // When no policy manifest, the original prototype.require is sustained
      return mod.require(path);
    };
  }

  /**
   * The `resolve` method that gets attached to module-scope `require`.
   * @param {string} request
   * @param {Parameters<Module['_resolveFilename']>[3]} options
   */
  function resolve(request, options) {
    validateString(request, 'request');
    return Module._resolveFilename(request, mod, false, options);
  }

  require.resolve = resolve;

  /**
   * The `paths` method that gets attached to module-scope `require`.
   * @param {string} request
   */
  function paths(request) {
    validateString(request, 'request');
    return Module._resolveLookupPaths(request, mod);
  }

  resolve.paths = paths;

  setOwnProperty(require, 'main', process.mainModule);

  // Enable support to add extra extension types.
  require.extensions = Module._extensions;

  require.cache = Module._cache;

  return require;
}

/**
 * Remove byte order marker. This catches EF BB BF (the UTF-8 BOM)
 * because the buffer-to-string conversion in `fs.readFileSync()`
 * translates it to FEFF, the UTF-16 BOM.
 * @param {string} content
 */
function stripBOM(content) {
  if (StringPrototypeCharCodeAt(content) === 0xFEFF) {
    content = StringPrototypeSlice(content, 1);
  }
  return content;
}

/**
 * Add built-in modules to a global or REPL scope object.
 * @param {Record<string, unknown>} object - The object such as `globalThis` to add the built-in modules to.
 * @param {string} dummyModuleName - The label representing the set of built-in modules to add.
 */
function addBuiltinLibsToObject(object, dummyModuleName) {
  // Make built-in modules available directly (loaded lazily).
  const Module = require('internal/modules/cjs/loader').Module;
  const { builtinModules } = Module;

  // To require built-in modules in user-land and ignore modules whose
  // `canBeRequiredByUsers` is false. So we create a dummy module object and not
  // use `require()` directly.
  const dummyModule = new Module(dummyModuleName);

  ArrayPrototypeForEach(builtinModules, (name) => {
    // Neither add underscored modules, nor ones that contain slashes (e.g.,
    // 'fs/promises') or ones that are already defined.
    if (StringPrototypeStartsWith(name, '_') ||
        StringPrototypeIncludes(name, '/') ||
        ObjectPrototypeHasOwnProperty(object, name)) {
      return;
    }
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

    ObjectDefineProperty(object, name, {
      __proto__: null,
      get: () => {
        const lib = dummyModule.require(name);

        try {
          // Override the current getter/setter and set up a new
          // non-enumerable property.
          ObjectDefineProperty(object, name, {
            __proto__: null,
            get: () => lib,
            set: setReal,
            configurable: true,
            enumerable: false,
          });
        } catch {
          // If the property is no longer configurable, ignore the error.
        }

        return lib;
      },
      set: setReal,
      configurable: true,
      enumerable: false,
    });
  });
}

/**
 * If a referrer is an URL instance or absolute path, convert it into an URL string.
 * @param {string | URL} referrer
 */
function normalizeReferrerURL(referrer) {
  if (typeof referrer === 'string' && path.isAbsolute(referrer)) {
    return pathToFileURL(referrer).href;
  }
  return new URL(referrer).href;
}

module.exports = {
  addBuiltinLibsToObject,
  getCjsConditions,
  initializeCjsConditions,
  loadBuiltinModule,
  makeRequireFunction,
  normalizeReferrerURL,
  stripBOM,
  toRealPath,
};
