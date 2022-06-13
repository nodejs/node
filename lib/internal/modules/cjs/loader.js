// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeConcat,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypeIndexOf,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  ArrayPrototypeUnshift,
  ArrayPrototypeUnshiftApply,
  Boolean,
  Error,
  JSONParse,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototype,
  ObjectPrototypeHasOwnProperty,
  ObjectSetPrototypeOf,
  Proxy,
  ReflectApply,
  ReflectSet,
  RegExpPrototypeExec,
  RegExpPrototypeTest,
  SafeMap,
  SafeWeakMap,
  String,
  StringPrototypeCharAt,
  StringPrototypeCharCodeAt,
  StringPrototypeEndsWith,
  StringPrototypeLastIndexOf,
  StringPrototypeIndexOf,
  StringPrototypeMatch,
  StringPrototypeRepeat,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;

// Map used to store CJS parsing data.
const cjsParseCache = new SafeWeakMap();

// Set first due to cycle with ESM loader functions.
module.exports = {
  wrapSafe, Module, toRealPath, readPackageScope, cjsParseCache,
  get hasLoadedAnyUserCJSModule() { return hasLoadedAnyUserCJSModule; }
};

const { NativeModule } = require('internal/bootstrap/loaders');
const {
  maybeCacheSourceMap,
} = require('internal/source_map/source_map_cache');
const { pathToFileURL, fileURLToPath, isURLInstance } = require('internal/url');
const { deprecate } = require('internal/util');
const vm = require('vm');
const assert = require('internal/assert');
const fs = require('fs');
const internalFS = require('internal/fs/utils');
const path = require('path');
const { sep } = path;
const { internalModuleStat } = internalBinding('fs');
const packageJsonReader = require('internal/modules/package_json_reader');
const { safeGetenv } = internalBinding('credentials');
const {
  cjsConditions,
  hasEsmSyntax,
  loadNativeModule,
  makeRequireFunction,
  normalizeReferrerURL,
  stripBOM,
} = require('internal/modules/cjs/helpers');
const { getOptionValue } = require('internal/options');
const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
// Do not eagerly grab .manifest, it may be in TDZ
const policy = getOptionValue('--experimental-policy') ?
  require('internal/process/policy') :
  null;

// Whether any user-provided CJS modules had been loaded (executed).
// Used for internal assertions.
let hasLoadedAnyUserCJSModule = false;

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_MODULE_SPECIFIER,
    ERR_REQUIRE_ESM,
    ERR_UNKNOWN_BUILTIN_MODULE,
  },
  setArrowMessage,
} = require('internal/errors');
const { validateString } = require('internal/validators');
const pendingDeprecation = getOptionValue('--pending-deprecation');

const {
  CHAR_FORWARD_SLASH,
  CHAR_BACKWARD_SLASH,
  CHAR_COLON
} = require('internal/constants');

const {
  isProxy
} = require('internal/util/types');

const asyncESM = require('internal/process/esm_loader');
const { enrichCJSError } = require('internal/modules/esm/translators');
const { kEvaluated } = internalBinding('module_wrap');
const {
  encodedSepRegEx,
  packageExportsResolve,
  packageImportsResolve
} = require('internal/modules/esm/resolve');

const isWindows = process.platform === 'win32';

const relativeResolveCache = ObjectCreate(null);

let requireDepth = 0;
let statCache = null;
let isPreloading = false;

function stat(filename) {
  filename = path.toNamespacedPath(filename);
  if (statCache !== null) {
    const result = statCache.get(filename);
    if (result !== undefined) return result;
  }
  const result = internalModuleStat(filename);
  if (statCache !== null && result >= 0) {
    // Only set cache when `internalModuleStat(filename)` succeeds.
    statCache.set(filename, result);
  }
  return result;
}

function updateChildren(parent, child, scan) {
  const children = parent?.children;
  if (children && !(scan && ArrayPrototypeIncludes(children, child)))
    ArrayPrototypePush(children, child);
}

const moduleParentCache = new SafeWeakMap();
function Module(id = '', parent) {
  this.id = id;
  this.path = path.dirname(id);
  this.exports = {};
  moduleParentCache.set(this, parent);
  updateChildren(parent, this, false);
  this.filename = null;
  this.loaded = false;
  this.children = [];
}

const builtinModules = [];
for (const { 0: id, 1: mod } of NativeModule.map) {
  if (mod.canBeRequiredByUsers &&
    NativeModule.canBeRequiredWithoutScheme(id)) {
    ArrayPrototypePush(builtinModules, id);
  }
}

ObjectFreeze(builtinModules);
Module.builtinModules = builtinModules;

Module._cache = ObjectCreate(null);
Module._pathCache = ObjectCreate(null);
Module._extensions = ObjectCreate(null);
let modulePaths = [];
Module.globalPaths = [];

let patched = false;

// eslint-disable-next-line func-style
let wrap = function(script) {
  return Module.wrapper[0] + script + Module.wrapper[1];
};

const wrapper = [
  '(function (exports, require, module, __filename, __dirname) { ',
  '\n});',
];

let wrapperProxy = new Proxy(wrapper, {
  set(target, property, value, receiver) {
    patched = true;
    return ReflectSet(target, property, value, receiver);
  },

  defineProperty(target, property, descriptor) {
    patched = true;
    return ObjectDefineProperty(target, property, descriptor);
  }
});

ObjectDefineProperty(Module, 'wrap', {
  __proto__: null,
  get() {
    return wrap;
  },

  set(value) {
    patched = true;
    wrap = value;
  }
});

ObjectDefineProperty(Module, 'wrapper', {
  __proto__: null,
  get() {
    return wrapperProxy;
  },

  set(value) {
    patched = true;
    wrapperProxy = value;
  }
});

const isPreloadingDesc = { get() { return isPreloading; } };
ObjectDefineProperty(Module.prototype, 'isPreloading', isPreloadingDesc);
ObjectDefineProperty(NativeModule.prototype, 'isPreloading', isPreloadingDesc);

function getModuleParent() {
  return moduleParentCache.get(this);
}

function setModuleParent(value) {
  moduleParentCache.set(this, value);
}

ObjectDefineProperty(Module.prototype, 'parent', {
  __proto__: null,
  get: pendingDeprecation ? deprecate(
    getModuleParent,
    'module.parent is deprecated due to accuracy issues. Please use ' +
      'require.main to find program entry point instead.',
    'DEP0144'
  ) : getModuleParent,
  set: pendingDeprecation ? deprecate(
    setModuleParent,
    'module.parent is deprecated due to accuracy issues. Please use ' +
      'require.main to find program entry point instead.',
    'DEP0144'
  ) : setModuleParent,
});

let debug = require('internal/util/debuglog').debuglog('module', (fn) => {
  debug = fn;
});
Module._debug = deprecate(debug, 'Module._debug is deprecated.', 'DEP0077');

// Given a module name, and a list of paths to test, returns the first
// matching file in the following precedence.
//
// require("a.<ext>")
//   -> a.<ext>
//
// require("a")
//   -> a
//   -> a.<ext>
//   -> a/index.<ext>

const packageJsonCache = new SafeMap();

function readPackage(requestPath) {
  const jsonPath = path.resolve(requestPath, 'package.json');

  const existing = packageJsonCache.get(jsonPath);
  if (existing !== undefined) return existing;

  const result = packageJsonReader.read(jsonPath);
  const json = result.containsKeys === false ? '{}' : result.string;
  if (json === undefined) {
    packageJsonCache.set(jsonPath, false);
    return false;
  }

  try {
    const parsed = JSONParse(json);
    const filtered = {
      name: parsed.name,
      main: parsed.main,
      exports: parsed.exports,
      imports: parsed.imports,
      type: parsed.type
    };
    packageJsonCache.set(jsonPath, filtered);
    return filtered;
  } catch (e) {
    e.path = jsonPath;
    e.message = 'Error parsing ' + jsonPath + ': ' + e.message;
    throw e;
  }
}

function readPackageScope(checkPath) {
  const rootSeparatorIndex = StringPrototypeIndexOf(checkPath, sep);
  let separatorIndex;
  do {
    separatorIndex = StringPrototypeLastIndexOf(checkPath, sep);
    checkPath = StringPrototypeSlice(checkPath, 0, separatorIndex);
    if (StringPrototypeEndsWith(checkPath, sep + 'node_modules'))
      return false;
    const pjson = readPackage(checkPath + sep);
    if (pjson) return {
      data: pjson,
      path: checkPath,
    };
  } while (separatorIndex > rootSeparatorIndex);
  return false;
}

function tryPackage(requestPath, exts, isMain, originalPath) {
  const pkg = readPackage(requestPath)?.main;

  if (!pkg) {
    return tryExtensions(path.resolve(requestPath, 'index'), exts, isMain);
  }

  const filename = path.resolve(requestPath, pkg);
  let actual = tryFile(filename, isMain) ||
    tryExtensions(filename, exts, isMain) ||
    tryExtensions(path.resolve(filename, 'index'), exts, isMain);
  if (actual === false) {
    actual = tryExtensions(path.resolve(requestPath, 'index'), exts, isMain);
    if (!actual) {
      // eslint-disable-next-line no-restricted-syntax
      const err = new Error(
        `Cannot find module '${filename}'. ` +
        'Please verify that the package.json has a valid "main" entry'
      );
      err.code = 'MODULE_NOT_FOUND';
      err.path = path.resolve(requestPath, 'package.json');
      err.requestPath = originalPath;
      // TODO(BridgeAR): Add the requireStack as well.
      throw err;
    } else {
      const jsonPath = path.resolve(requestPath, 'package.json');
      process.emitWarning(
        `Invalid 'main' field in '${jsonPath}' of '${pkg}'. ` +
          'Please either fix that or report it to the module author',
        'DeprecationWarning',
        'DEP0128'
      );
    }
  }
  return actual;
}

// In order to minimize unnecessary lstat() calls,
// this cache is a list of known-real paths.
// Set to an empty Map to reset.
const realpathCache = new SafeMap();

// Check if the file exists and is not a directory
// if using --preserve-symlinks and isMain is false,
// keep symlinks intact, otherwise resolve to the
// absolute realpath.
function tryFile(requestPath, isMain) {
  const rc = stat(requestPath);
  if (rc !== 0) return;
  if (preserveSymlinks && !isMain) {
    return path.resolve(requestPath);
  }
  return toRealPath(requestPath);
}

function toRealPath(requestPath) {
  return fs.realpathSync(requestPath, {
    [internalFS.realpathCacheKey]: realpathCache
  });
}

// Given a path, check if the file exists with any of the set extensions
function tryExtensions(p, exts, isMain) {
  for (let i = 0; i < exts.length; i++) {
    const filename = tryFile(p + exts[i], isMain);

    if (filename) {
      return filename;
    }
  }
  return false;
}

// Find the longest (possibly multi-dot) extension registered in
// Module._extensions
function findLongestRegisteredExtension(filename) {
  const name = path.basename(filename);
  let currentExtension;
  let index;
  let startIndex = 0;
  while ((index = StringPrototypeIndexOf(name, '.', startIndex)) !== -1) {
    startIndex = index + 1;
    if (index === 0) continue; // Skip dotfiles like .gitignore
    currentExtension = StringPrototypeSlice(name, index);
    if (Module._extensions[currentExtension]) return currentExtension;
  }
  return '.js';
}

function trySelfParentPath(parent) {
  if (!parent) return false;

  if (parent.filename) {
    return parent.filename;
  } else if (parent.id === '<repl>' || parent.id === 'internal/preload') {
    try {
      return process.cwd() + path.sep;
    } catch {
      return false;
    }
  }
}

function trySelf(parentPath, request) {
  if (!parentPath) return false;

  const { data: pkg, path: pkgPath } = readPackageScope(parentPath) || {};
  if (!pkg || pkg.exports === undefined) return false;
  if (typeof pkg.name !== 'string') return false;

  let expansion;
  if (request === pkg.name) {
    expansion = '.';
  } else if (StringPrototypeStartsWith(request, `${pkg.name}/`)) {
    expansion = '.' + StringPrototypeSlice(request, pkg.name.length);
  } else {
    return false;
  }

  try {
    return finalizeEsmResolution(packageExportsResolve(
      pathToFileURL(pkgPath + '/package.json'), expansion, pkg,
      pathToFileURL(parentPath), cjsConditions), parentPath, pkgPath);
  } catch (e) {
    if (e.code === 'ERR_MODULE_NOT_FOUND')
      throw createEsmNotFoundErr(request, pkgPath + '/package.json');
    throw e;
  }
}

// This only applies to requests of a specific form:
// 1. name/.*
// 2. @scope/name/.*
const EXPORTS_PATTERN = /^((?:@[^/\\%]+\/)?[^./\\%][^/\\%]*)(\/.*)?$/;
function resolveExports(nmPath, request) {
  // The implementation's behavior is meant to mirror resolution in ESM.
  const { 1: name, 2: expansion = '' } =
    StringPrototypeMatch(request, EXPORTS_PATTERN) || [];
  if (!name)
    return;
  const pkgPath = path.resolve(nmPath, name);
  const pkg = readPackage(pkgPath);
  if (pkg?.exports != null) {
    try {
      return finalizeEsmResolution(packageExportsResolve(
        pathToFileURL(pkgPath + '/package.json'), '.' + expansion, pkg, null,
        cjsConditions), null, pkgPath);
    } catch (e) {
      if (e.code === 'ERR_MODULE_NOT_FOUND')
        throw createEsmNotFoundErr(request, pkgPath + '/package.json');
      throw e;
    }
  }
}

const trailingSlashRegex = /(?:^|\/)\.?\.$/;
Module._findPath = function(request, paths, isMain) {
  const absoluteRequest = path.isAbsolute(request);
  if (absoluteRequest) {
    paths = [''];
  } else if (!paths || paths.length === 0) {
    return false;
  }

  const cacheKey = request + '\x00' + ArrayPrototypeJoin(paths, '\x00');
  const entry = Module._pathCache[cacheKey];
  if (entry)
    return entry;

  let exts;
  let trailingSlash = request.length > 0 &&
    StringPrototypeCharCodeAt(request, request.length - 1) ===
    CHAR_FORWARD_SLASH;
  if (!trailingSlash) {
    trailingSlash = RegExpPrototypeTest(trailingSlashRegex, request);
  }

  // For each path
  for (let i = 0; i < paths.length; i++) {
    // Don't search further if path doesn't exist
    const curPath = paths[i];
    if (curPath && stat(curPath) < 1) continue;

    if (!absoluteRequest) {
      const exportsResolved = resolveExports(curPath, request);
      if (exportsResolved)
        return exportsResolved;
    }

    const basePath = path.resolve(curPath, request);
    let filename;

    const rc = stat(basePath);
    if (!trailingSlash) {
      if (rc === 0) {  // File.
        if (!isMain) {
          if (preserveSymlinks) {
            filename = path.resolve(basePath);
          } else {
            filename = toRealPath(basePath);
          }
        } else if (preserveSymlinksMain) {
          // For the main module, we use the preserveSymlinksMain flag instead
          // mainly for backward compatibility, as the preserveSymlinks flag
          // historically has not applied to the main module.  Most likely this
          // was intended to keep .bin/ binaries working, as following those
          // symlinks is usually required for the imports in the corresponding
          // files to resolve; that said, in some use cases following symlinks
          // causes bigger problems which is why the preserveSymlinksMain option
          // is needed.
          filename = path.resolve(basePath);
        } else {
          filename = toRealPath(basePath);
        }
      }

      if (!filename) {
        // Try it with each of the extensions
        if (exts === undefined)
          exts = ObjectKeys(Module._extensions);
        filename = tryExtensions(basePath, exts, isMain);
      }
    }

    if (!filename && rc === 1) {  // Directory.
      // try it with each of the extensions at "index"
      if (exts === undefined)
        exts = ObjectKeys(Module._extensions);
      filename = tryPackage(basePath, exts, isMain, request);
    }

    if (filename) {
      Module._pathCache[cacheKey] = filename;
      return filename;
    }
  }

  return false;
};

// 'node_modules' character codes reversed
const nmChars = [ 115, 101, 108, 117, 100, 111, 109, 95, 101, 100, 111, 110 ];
const nmLen = nmChars.length;
if (isWindows) {
  // 'from' is the __dirname of the module.
  Module._nodeModulePaths = function(from) {
    // Guarantee that 'from' is absolute.
    from = path.resolve(from);

    // note: this approach *only* works when the path is guaranteed
    // to be absolute.  Doing a fully-edge-case-correct path.split
    // that works on both Windows and Posix is non-trivial.

    // return root node_modules when path is 'D:\\'.
    // path.resolve will make sure from.length >=3 in Windows.
    if (StringPrototypeCharCodeAt(from, from.length - 1) ===
          CHAR_BACKWARD_SLASH &&
        StringPrototypeCharCodeAt(from, from.length - 2) === CHAR_COLON)
      return [from + 'node_modules'];

    const paths = [];
    for (let i = from.length - 1, p = 0, last = from.length; i >= 0; --i) {
      const code = StringPrototypeCharCodeAt(from, i);
      // The path segment separator check ('\' and '/') was used to get
      // node_modules path for every path segment.
      // Use colon as an extra condition since we can get node_modules
      // path for drive root like 'C:\node_modules' and don't need to
      // parse drive name.
      if (code === CHAR_BACKWARD_SLASH ||
          code === CHAR_FORWARD_SLASH ||
          code === CHAR_COLON) {
        if (p !== nmLen)
          ArrayPrototypePush(
            paths,
            StringPrototypeSlice(from, 0, last) + '\\node_modules'
          );
        last = i;
        p = 0;
      } else if (p !== -1) {
        if (nmChars[p] === code) {
          ++p;
        } else {
          p = -1;
        }
      }
    }

    return paths;
  };
} else { // posix
  // 'from' is the __dirname of the module.
  Module._nodeModulePaths = function(from) {
    // Guarantee that 'from' is absolute.
    from = path.resolve(from);
    // Return early not only to avoid unnecessary work, but to *avoid* returning
    // an array of two items for a root: [ '//node_modules', '/node_modules' ]
    if (from === '/')
      return ['/node_modules'];

    // note: this approach *only* works when the path is guaranteed
    // to be absolute.  Doing a fully-edge-case-correct path.split
    // that works on both Windows and Posix is non-trivial.
    const paths = [];
    for (let i = from.length - 1, p = 0, last = from.length; i >= 0; --i) {
      const code = StringPrototypeCharCodeAt(from, i);
      if (code === CHAR_FORWARD_SLASH) {
        if (p !== nmLen)
          ArrayPrototypePush(
            paths,
            StringPrototypeSlice(from, 0, last) + '/node_modules'
          );
        last = i;
        p = 0;
      } else if (p !== -1) {
        if (nmChars[p] === code) {
          ++p;
        } else {
          p = -1;
        }
      }
    }

    // Append /node_modules to handle root paths.
    ArrayPrototypePush(paths, '/node_modules');

    return paths;
  };
}

Module._resolveLookupPaths = function(request, parent) {
  if (NativeModule.canBeRequiredByUsers(request) &&
      NativeModule.canBeRequiredWithoutScheme(request)) {
    debug('looking for %j in []', request);
    return null;
  }

  // Check for node modules paths.
  if (StringPrototypeCharAt(request, 0) !== '.' ||
      (request.length > 1 &&
      StringPrototypeCharAt(request, 1) !== '.' &&
      StringPrototypeCharAt(request, 1) !== '/' &&
      (!isWindows || StringPrototypeCharAt(request, 1) !== '\\'))) {

    let paths = modulePaths;
    if (parent?.paths?.length) {
      paths = ArrayPrototypeConcat(parent.paths, paths);
    }

    debug('looking for %j in %j', request, paths);
    return paths.length > 0 ? paths : null;
  }

  // In REPL, parent.filename is null.
  if (!parent || !parent.id || !parent.filename) {
    // Make require('./path/to/foo') work - normally the path is taken
    // from realpath(__filename) but in REPL there is no filename
    const mainPaths = ['.'];

    debug('looking for %j in %j', request, mainPaths);
    return mainPaths;
  }

  debug('RELATIVE: requested: %s from parent.id %s', request, parent.id);

  const parentDir = [path.dirname(parent.filename)];
  debug('looking for %j', parentDir);
  return parentDir;
};

function emitCircularRequireWarning(prop) {
  process.emitWarning(
    `Accessing non-existent property '${String(prop)}' of module exports ` +
    'inside circular dependency'
  );
}

// A Proxy that can be used as the prototype of a module.exports object and
// warns when non-existent properties are accessed.
const CircularRequirePrototypeWarningProxy = new Proxy({}, {
  get(target, prop) {
    // Allow __esModule access in any case because it is used in the output
    // of transpiled code to determine whether something comes from an
    // ES module, and is not used as a regular key of `module.exports`.
    if (prop in target || prop === '__esModule') return target[prop];
    emitCircularRequireWarning(prop);
    return undefined;
  },

  getOwnPropertyDescriptor(target, prop) {
    if (ObjectPrototypeHasOwnProperty(target, prop) || prop === '__esModule')
      return ObjectGetOwnPropertyDescriptor(target, prop);
    emitCircularRequireWarning(prop);
    return undefined;
  }
});

function getExportsForCircularRequire(module) {
  if (module.exports &&
      !isProxy(module.exports) &&
      ObjectGetPrototypeOf(module.exports) === ObjectPrototype &&
      // Exclude transpiled ES6 modules / TypeScript code because those may
      // employ unusual patterns for accessing 'module.exports'. That should
      // be okay because ES6 modules have a different approach to circular
      // dependencies anyway.
      !module.exports.__esModule) {
    // This is later unset once the module is done loading.
    ObjectSetPrototypeOf(
      module.exports, CircularRequirePrototypeWarningProxy);
  }

  return module.exports;
}

// Check the cache for the requested file.
// 1. If a module already exists in the cache: return its exports object.
// 2. If the module is native: call
//    `NativeModule.prototype.compileForPublicLoader()` and return the exports.
// 3. Otherwise, create a new module for the file and save it to the cache.
//    Then have it load  the file contents before returning its exports
//    object.
Module._load = function(request, parent, isMain) {
  let relResolveCacheIdentifier;
  if (parent) {
    debug('Module._load REQUEST %s parent: %s', request, parent.id);
    // Fast path for (lazy loaded) modules in the same directory. The indirect
    // caching is required to allow cache invalidation without changing the old
    // cache key names.
    relResolveCacheIdentifier = `${parent.path}\x00${request}`;
    const filename = relativeResolveCache[relResolveCacheIdentifier];
    if (filename !== undefined) {
      const cachedModule = Module._cache[filename];
      if (cachedModule !== undefined) {
        updateChildren(parent, cachedModule, true);
        if (!cachedModule.loaded)
          return getExportsForCircularRequire(cachedModule);
        return cachedModule.exports;
      }
      delete relativeResolveCache[relResolveCacheIdentifier];
    }
  }

  const filename = Module._resolveFilename(request, parent, isMain);
  if (StringPrototypeStartsWith(filename, 'node:')) {
    // Slice 'node:' prefix
    const id = StringPrototypeSlice(filename, 5);

    const module = loadNativeModule(id, request);
    if (!module?.canBeRequiredByUsers) {
      throw new ERR_UNKNOWN_BUILTIN_MODULE(filename);
    }

    return module.exports;
  }

  const cachedModule = Module._cache[filename];
  if (cachedModule !== undefined) {
    updateChildren(parent, cachedModule, true);
    if (!cachedModule.loaded) {
      const parseCachedModule = cjsParseCache.get(cachedModule);
      if (!parseCachedModule || parseCachedModule.loaded)
        return getExportsForCircularRequire(cachedModule);
      parseCachedModule.loaded = true;
    } else {
      return cachedModule.exports;
    }
  }

  const mod = loadNativeModule(filename, request);
  if (mod?.canBeRequiredByUsers &&
      NativeModule.canBeRequiredWithoutScheme(filename)) {
    return mod.exports;
  }

  // Don't call updateChildren(), Module constructor already does.
  const module = cachedModule || new Module(filename, parent);

  if (isMain) {
    process.mainModule = module;
    module.id = '.';
  }

  Module._cache[filename] = module;
  if (parent !== undefined) {
    relativeResolveCache[relResolveCacheIdentifier] = filename;
  }

  let threw = true;
  try {
    module.load(filename);
    threw = false;
  } finally {
    if (threw) {
      delete Module._cache[filename];
      if (parent !== undefined) {
        delete relativeResolveCache[relResolveCacheIdentifier];
        const children = parent?.children;
        if (ArrayIsArray(children)) {
          const index = ArrayPrototypeIndexOf(children, module);
          if (index !== -1) {
            ArrayPrototypeSplice(children, index, 1);
          }
        }
      }
    } else if (module.exports &&
               !isProxy(module.exports) &&
               ObjectGetPrototypeOf(module.exports) ===
                 CircularRequirePrototypeWarningProxy) {
      ObjectSetPrototypeOf(module.exports, ObjectPrototype);
    }
  }

  return module.exports;
};

Module._resolveFilename = function(request, parent, isMain, options) {
  if (StringPrototypeStartsWith(request, 'node:') ||
      (NativeModule.canBeRequiredByUsers(request) &&
      NativeModule.canBeRequiredWithoutScheme(request))) {
    return request;
  }

  let paths;

  if (typeof options === 'object' && options !== null) {
    if (ArrayIsArray(options.paths)) {
      const isRelative = StringPrototypeStartsWith(request, './') ||
          StringPrototypeStartsWith(request, '../') ||
          ((isWindows && StringPrototypeStartsWith(request, '.\\')) ||
          StringPrototypeStartsWith(request, '..\\'));

      if (isRelative) {
        paths = options.paths;
      } else {
        const fakeParent = new Module('', null);

        paths = [];

        for (let i = 0; i < options.paths.length; i++) {
          const path = options.paths[i];
          fakeParent.paths = Module._nodeModulePaths(path);
          const lookupPaths = Module._resolveLookupPaths(request, fakeParent);

          for (let j = 0; j < lookupPaths.length; j++) {
            if (!ArrayPrototypeIncludes(paths, lookupPaths[j]))
              ArrayPrototypePush(paths, lookupPaths[j]);
          }
        }
      }
    } else if (options.paths === undefined) {
      paths = Module._resolveLookupPaths(request, parent);
    } else {
      throw new ERR_INVALID_ARG_VALUE('options.paths', options.paths);
    }
  } else {
    paths = Module._resolveLookupPaths(request, parent);
  }

  if (parent?.filename) {
    if (request[0] === '#') {
      const pkg = readPackageScope(parent.filename) || {};
      if (pkg.data?.imports != null) {
        try {
          return finalizeEsmResolution(
            packageImportsResolve(request, pathToFileURL(parent.filename),
                                  cjsConditions), parent.filename,
            pkg.path);
        } catch (e) {
          if (e.code === 'ERR_MODULE_NOT_FOUND')
            throw createEsmNotFoundErr(request);
          throw e;
        }
      }
    }
  }

  // Try module self resolution first
  const parentPath = trySelfParentPath(parent);
  const selfResolved = trySelf(parentPath, request);
  if (selfResolved) {
    const cacheKey = request + '\x00' +
         (paths.length === 1 ? paths[0] : ArrayPrototypeJoin(paths, '\x00'));
    Module._pathCache[cacheKey] = selfResolved;
    return selfResolved;
  }

  // Look up the filename first, since that's the cache key.
  const filename = Module._findPath(request, paths, isMain, false);
  if (filename) return filename;
  const requireStack = [];
  for (let cursor = parent;
    cursor;
    cursor = moduleParentCache.get(cursor)) {
    ArrayPrototypePush(requireStack, cursor.filename || cursor.id);
  }
  let message = `Cannot find module '${request}'`;
  if (requireStack.length > 0) {
    message = message + '\nRequire stack:\n- ' +
              ArrayPrototypeJoin(requireStack, '\n- ');
  }
  // eslint-disable-next-line no-restricted-syntax
  const err = new Error(message);
  err.code = 'MODULE_NOT_FOUND';
  err.requireStack = requireStack;
  throw err;
};

function finalizeEsmResolution(resolved, parentPath, pkgPath) {
  if (RegExpPrototypeTest(encodedSepRegEx, resolved))
    throw new ERR_INVALID_MODULE_SPECIFIER(
      resolved, 'must not include encoded "/" or "\\" characters', parentPath);
  const filename = fileURLToPath(resolved);
  const actual = tryFile(filename);
  if (actual)
    return actual;
  const err = createEsmNotFoundErr(filename,
                                   path.resolve(pkgPath, 'package.json'));
  throw err;
}

function createEsmNotFoundErr(request, path) {
  // eslint-disable-next-line no-restricted-syntax
  const err = new Error(`Cannot find module '${request}'`);
  err.code = 'MODULE_NOT_FOUND';
  if (path)
    err.path = path;
  // TODO(BridgeAR): Add the requireStack as well.
  return err;
}

// Given a file name, pass it to the proper extension handler.
Module.prototype.load = function(filename) {
  debug('load %j for module %j', filename, this.id);

  assert(!this.loaded);
  this.filename = filename;
  this.paths = Module._nodeModulePaths(path.dirname(filename));

  const extension = findLongestRegisteredExtension(filename);
  // allow .mjs to be overridden
  if (StringPrototypeEndsWith(filename, '.mjs') && !Module._extensions['.mjs'])
    throw new ERR_REQUIRE_ESM(filename, true);

  Module._extensions[extension](this, filename);
  this.loaded = true;

  const esmLoader = asyncESM.esmLoader;
  // Create module entry at load time to snapshot exports correctly
  const exports = this.exports;
  // Preemptively cache
  if ((module?.module === undefined ||
       module.module.getStatus() < kEvaluated) &&
      !esmLoader.cjsCache.has(this))
    esmLoader.cjsCache.set(this, exports);
};


// Loads a module at the given file path. Returns that module's
// `exports` property.
Module.prototype.require = function(id) {
  validateString(id, 'id');
  if (id === '') {
    throw new ERR_INVALID_ARG_VALUE('id', id,
                                    'must be a non-empty string');
  }
  requireDepth++;
  try {
    return Module._load(id, this, /* isMain */ false);
  } finally {
    requireDepth--;
  }
};


// Resolved path to process.argv[1] will be lazily placed here
// (needed for setting breakpoint when called with --inspect-brk)
let resolvedArgv;
let hasPausedEntry = false;

function wrapSafe(filename, content, cjsModuleInstance) {
  if (patched) {
    const wrapper = Module.wrap(content);
    return vm.runInThisContext(wrapper, {
      filename,
      lineOffset: 0,
      displayErrors: true,
      importModuleDynamically: async (specifier, _, importAssertions) => {
        const loader = asyncESM.esmLoader;
        return loader.import(specifier, normalizeReferrerURL(filename),
                             importAssertions);
      },
    });
  }
  try {
    return vm.compileFunction(content, [
      'exports',
      'require',
      'module',
      '__filename',
      '__dirname',
    ], {
      filename,
      importModuleDynamically(specifier, _, importAssertions) {
        const loader = asyncESM.esmLoader;
        return loader.import(specifier, normalizeReferrerURL(filename),
                             importAssertions);
      },
    });
  } catch (err) {
    if (process.mainModule === cjsModuleInstance)
      enrichCJSError(err, content);
    throw err;
  }
}

// Run the file contents in the correct scope or sandbox. Expose
// the correct helper variables (require, module, exports) to
// the file.
// Returns exception, if any.
Module.prototype._compile = function(content, filename) {
  let moduleURL;
  let redirects;
  if (policy?.manifest) {
    moduleURL = pathToFileURL(filename);
    redirects = policy.manifest.getDependencyMapper(moduleURL);
    policy.manifest.assertIntegrity(moduleURL, content);
  }

  maybeCacheSourceMap(filename, content, this);
  const compiledWrapper = wrapSafe(filename, content, this);

  let inspectorWrapper = null;
  if (getOptionValue('--inspect-brk') && process._eval == null) {
    if (!resolvedArgv) {
      // We enter the repl if we're not given a filename argument.
      if (process.argv[1]) {
        try {
          resolvedArgv = Module._resolveFilename(process.argv[1], null, false);
        } catch {
          // We only expect this codepath to be reached in the case of a
          // preloaded module (it will fail earlier with the main entry)
          assert(ArrayIsArray(getOptionValue('--require')));
        }
      } else {
        resolvedArgv = 'repl';
      }
    }

    // Set breakpoint on module start
    if (resolvedArgv && !hasPausedEntry && filename === resolvedArgv) {
      hasPausedEntry = true;
      inspectorWrapper = internalBinding('inspector').callAndPauseOnStart;
    }
  }
  const dirname = path.dirname(filename);
  const require = makeRequireFunction(this, redirects);
  let result;
  const exports = this.exports;
  const thisValue = exports;
  const module = this;
  if (requireDepth === 0) statCache = new SafeMap();
  if (inspectorWrapper) {
    result = inspectorWrapper(compiledWrapper, thisValue, exports,
                              require, module, filename, dirname);
  } else {
    result = ReflectApply(compiledWrapper, thisValue,
                          [exports, require, module, filename, dirname]);
  }
  hasLoadedAnyUserCJSModule = true;
  if (requireDepth === 0) statCache = null;
  return result;
};

// Native extension for .js
Module._extensions['.js'] = function(module, filename) {
  // If already analyzed the source, then it will be cached.
  const cached = cjsParseCache.get(module);
  let content;
  if (cached?.source) {
    content = cached.source;
    cached.source = undefined;
  } else {
    content = fs.readFileSync(filename, 'utf8');
  }
  if (StringPrototypeEndsWith(filename, '.js')) {
    const pkg = readPackageScope(filename);
    // Function require shouldn't be used in ES modules.
    if (pkg?.data?.type === 'module') {
      const parent = moduleParentCache.get(module);
      const parentPath = parent?.filename;
      const packageJsonPath = path.resolve(pkg.path, 'package.json');
      const usesEsm = hasEsmSyntax(content);
      const err = new ERR_REQUIRE_ESM(filename, usesEsm, parentPath,
                                      packageJsonPath);
      // Attempt to reconstruct the parent require frame.
      if (Module._cache[parentPath]) {
        let parentSource;
        try {
          parentSource = fs.readFileSync(parentPath, 'utf8');
        } catch {
          // Continue regardless of error.
        }
        if (parentSource) {
          const errLine = StringPrototypeSplit(
            StringPrototypeSlice(err.stack, StringPrototypeIndexOf(
              err.stack, '    at ')), '\n', 1)[0];
          const { 1: line, 2: col } =
              RegExpPrototypeExec(/(\d+):(\d+)\)/, errLine) || [];
          if (line && col) {
            const srcLine = StringPrototypeSplit(parentSource, '\n')[line - 1];
            const frame = `${parentPath}:${line}\n${srcLine}\n${
              StringPrototypeRepeat(' ', col - 1)}^\n`;
            setArrowMessage(err, frame);
          }
        }
      }
      throw err;
    }
  }
  module._compile(content, filename);
};


// Native extension for .json
Module._extensions['.json'] = function(module, filename) {
  const content = fs.readFileSync(filename, 'utf8');

  if (policy?.manifest) {
    const moduleURL = pathToFileURL(filename);
    policy.manifest.assertIntegrity(moduleURL, content);
  }

  try {
    module.exports = JSONParse(stripBOM(content));
  } catch (err) {
    err.message = filename + ': ' + err.message;
    throw err;
  }
};


// Native extension for .node
Module._extensions['.node'] = function(module, filename) {
  if (policy?.manifest) {
    const content = fs.readFileSync(filename);
    const moduleURL = pathToFileURL(filename);
    policy.manifest.assertIntegrity(moduleURL, content);
  }
  // Be aware this doesn't use `content`
  return process.dlopen(module, path.toNamespacedPath(filename));
};

function createRequireFromPath(filename) {
  // Allow a directory to be passed as the filename
  const trailingSlash =
    StringPrototypeEndsWith(filename, '/') ||
    (isWindows && StringPrototypeEndsWith(filename, '\\'));

  const proxyPath = trailingSlash ?
    path.join(filename, 'noop.js') :
    filename;

  const m = new Module(proxyPath);
  m.filename = proxyPath;

  m.paths = Module._nodeModulePaths(m.path);
  return makeRequireFunction(m, null);
}

const createRequireError = 'must be a file URL object, file URL string, or ' +
  'absolute path string';

function createRequire(filename) {
  let filepath;

  if (isURLInstance(filename) ||
      (typeof filename === 'string' && !path.isAbsolute(filename))) {
    try {
      filepath = fileURLToPath(filename);
    } catch {
      throw new ERR_INVALID_ARG_VALUE('filename', filename,
                                      createRequireError);
    }
  } else if (typeof filename !== 'string') {
    throw new ERR_INVALID_ARG_VALUE('filename', filename, createRequireError);
  } else {
    filepath = filename;
  }
  return createRequireFromPath(filepath);
}

Module.createRequire = createRequire;

Module._initPaths = function() {
  const homeDir = isWindows ? process.env.USERPROFILE : safeGetenv('HOME');
  const nodePath = isWindows ? process.env.NODE_PATH : safeGetenv('NODE_PATH');

  // process.execPath is $PREFIX/bin/node except on Windows where it is
  // $PREFIX\node.exe where $PREFIX is the root of the Node.js installation.
  const prefixDir = isWindows ?
    path.resolve(process.execPath, '..') :
    path.resolve(process.execPath, '..', '..');

  const paths = [path.resolve(prefixDir, 'lib', 'node')];

  if (homeDir) {
    ArrayPrototypeUnshift(paths, path.resolve(homeDir, '.node_libraries'));
    ArrayPrototypeUnshift(paths, path.resolve(homeDir, '.node_modules'));
  }

  if (nodePath) {
    ArrayPrototypeUnshiftApply(paths, ArrayPrototypeFilter(
      StringPrototypeSplit(nodePath, path.delimiter),
      Boolean
    ));
  }

  modulePaths = paths;

  // Clone as a shallow copy, for introspection.
  Module.globalPaths = ArrayPrototypeSlice(modulePaths);
};

Module._preloadModules = function(requests) {
  if (!ArrayIsArray(requests))
    return;

  isPreloading = true;

  // Preloaded modules have a dummy parent module which is deemed to exist
  // in the current working directory. This seeds the search path for
  // preloaded modules.
  const parent = new Module('internal/preload', null);
  try {
    parent.paths = Module._nodeModulePaths(process.cwd());
  } catch (e) {
    if (e.code !== 'ENOENT') {
      isPreloading = false;
      throw e;
    }
  }
  for (let n = 0; n < requests.length; n++)
    parent.require(requests[n]);
  isPreloading = false;
};

Module.syncBuiltinESMExports = function syncBuiltinESMExports() {
  for (const mod of NativeModule.map.values()) {
    if (mod.canBeRequiredByUsers &&
        NativeModule.canBeRequiredWithoutScheme(mod.id)) {
      mod.syncExports();
    }
  }
};

// Backwards compatibility
Module.Module = Module;
