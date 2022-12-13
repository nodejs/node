'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeSome,
  ObjectGetPrototypeOf,
  SafeMap,
  SafeSet,
  StringPrototypeCharCodeAt,
  StringPrototypeSlice,
  SyntaxErrorPrototype,
} = primordials;
const {
  ERR_MANIFEST_DEPENDENCY_MISSING,
  ERR_UNKNOWN_BUILTIN_MODULE
} = require('internal/errors').codes;
const { BuiltinModule } = require('internal/bootstrap/loaders');

const { validateString } = require('internal/validators');
const path = require('path');
const { toNamespacedPath } = path;
const { emitWarningSync } = require('internal/process/warning');
const { readFileSync } = require('fs');
const { internalModuleReadJSON } = internalBinding('fs');
const { pathToFileURL, fileURLToPath, URL } = require('url');

const { getOptionValue } = require('internal/options');
const { setOwnProperty } = require('internal/util');

let debug = require('internal/util/debuglog').debuglog('module', (fn) => {
  debug = fn;
});

let cjsConditions;
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

function getCjsConditions() {
  if (cjsConditions === undefined) {
    initializeCjsConditions();
  }
  return cjsConditions;
}

function loadBuiltinModule(filename, request) {
  const mod = BuiltinModule.map.get(filename);
  if (mod?.canBeRequiredByUsers) {
    debug('load built-in module %s', request);
    // compileForPublicLoader() throws if mod.canBeRequiredByUsers is false:
    mod.compileForPublicLoader();
    return mod;
  }
}

// Invoke with makeRequireFunction(module) where |module| is the Module object
// to use as the context for the require() function.
// Use redirects to set up a mapping from a policy and restrict dependencies
const urlToFileCache = new SafeMap();
function makeRequireFunction(mod, redirects) {
  const Module = mod.constructor;

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
        const href = destination.href;
        if (destination.protocol === 'node:') {
          const specifier = destination.pathname;
          const mod = loadBuiltinModule(specifier, href);
          if (mod && mod.canBeRequiredByUsers) {
            return mod.exports;
          }
          throw new ERR_UNKNOWN_BUILTIN_MODULE(specifier);
        } else if (destination.protocol === 'file:') {
          let filepath;
          if (urlToFileCache.has(href)) {
            filepath = urlToFileCache.get(href);
          } else {
            filepath = fileURLToPath(destination);
            urlToFileCache.set(href, filepath);
          }
          return mod.require(filepath);
        }
      }
      if (missing) {
        reaction(new ERR_MANIFEST_DEPENDENCY_MISSING(
          id,
          specifier,
          ArrayPrototypeJoin([...conditions], ', ')
        ));
      }
      return mod.require(specifier);
    };
  } else {
    require = function require(path) {
      return mod.require(path);
    };
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
 */
function stripBOM(content) {
  if (StringPrototypeCharCodeAt(content) === 0xFEFF) {
    content = StringPrototypeSlice(content, 1);
  }
  return content;
}

/**
 *
 * @param {string | URL} referrer
 * @returns {string}
 */
function normalizeReferrerURL(referrer) {
  if (typeof referrer === 'string' && path.isAbsolute(referrer)) {
    return pathToFileURL(referrer).href;
  }
  return new URL(referrer).href;
}

// For error messages only - used to check if ESM syntax is in use.
function hasEsmSyntax(code) {
  debug('Checking for ESM syntax');
  const parser = require('internal/deps/acorn/acorn/dist/acorn').Parser;
  let root;
  try {
    root = parser.parse(code, { sourceType: 'module', ecmaVersion: 'latest' });
  } catch {
    return false;
  }

  return ArrayPrototypeSome(root.body, (stmt) =>
    stmt.type === 'ExportDefaultDeclaration' ||
    stmt.type === 'ExportNamedDeclaration' ||
    stmt.type === 'ImportDeclaration' ||
    stmt.type === 'ExportAllDeclaration');
}

/**
 * @param {Error | any} err
 * @param {string} [content] Content of the file, if known.
 * @param {string} [filename] Useful only if `content` is unknown.
 */
function enrichCJSError(err, content, filename) {
  if (err != null && ObjectGetPrototypeOf(err) === SyntaxErrorPrototype &&
      hasEsmSyntax(content || readFileSync(filename, 'utf-8'))) {
    // Emit the warning synchronously because we are in the middle of handling
    // a SyntaxError that will throw and likely terminate the process before an
    // asynchronous warning would be emitted.
    emitWarningSync(
      'To load an ES module, set "type": "module" in the package.json or use ' +
      'the .mjs extension.'
    );
  }
}

const packageJSONCache = new SafeMap();

let manifest;
/**
 *
 * @param {string} jsonPath
 */
function readPackageJSON(jsonPath) {
  if (packageJSONCache.has(jsonPath)) {
    return packageJSONCache.get(jsonPath);
  }

  const { 0: string, 1: containsKeys } = internalModuleReadJSON(
    toNamespacedPath(jsonPath)
  );
  const result = { string, containsKeys };
  if (string !== undefined) {
    if (manifest === undefined) {
      manifest = getOptionValue('--experimental-policy') ?
        require('internal/process/policy').manifest :
        null;
    }
    if (manifest !== null) {
      const jsonURL = pathToFileURL(jsonPath);
      manifest.assertIntegrity(jsonURL, string);
    }
  }
  packageJSONCache.set(jsonPath, result);
  return result;
}

module.exports = {
  getCjsConditions,
  initializeCjsConditions,
  hasEsmSyntax,
  loadBuiltinModule,
  makeRequireFunction,
  normalizeReferrerURL,
  stripBOM,
  enrichCJSError,
  readPackageJSON,
};
