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
  Error,
  JSONParse,
  Map,
  Number,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetPrototypeOf,
  ObjectIs,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  ObjectSetPrototypeOf,
  ReflectSet,
  RegExpPrototypeTest,
  SafeMap,
  String,
  StringPrototypeIndexOf,
  StringPrototypeMatch,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

const { NativeModule } = require('internal/bootstrap/loaders');
const {
  maybeCacheSourceMap,
  rekeySourceMap
} = require('internal/source_map/source_map_cache');
const { pathToFileURL, fileURLToPath, URL } = require('internal/url');
const { deprecate } = require('internal/util');
const vm = require('vm');
const assert = require('internal/assert');
const fs = require('fs');
const internalFS = require('internal/fs/utils');
const path = require('path');
const { emitWarningSync } = require('internal/process/warning');
const {
  internalModuleReadJSON,
  internalModuleStat
} = internalBinding('fs');
const { safeGetenv } = internalBinding('credentials');
const {
  makeRequireFunction,
  normalizeReferrerURL,
  stripBOM,
  loadNativeModule
} = require('internal/modules/cjs/helpers');
const { getOptionValue } = require('internal/options');
const enableSourceMaps = getOptionValue('--enable-source-maps');
const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const manifest = getOptionValue('--experimental-policy') ?
  require('internal/process/policy').manifest :
  null;
const { compileFunction } = internalBinding('contextify');

// Whether any user-provided CJS modules had been loaded (executed).
// Used for internal assertions.
let hasLoadedAnyUserCJSModule = false;

const {
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_OPT_VALUE,
  ERR_INVALID_PACKAGE_CONFIG,
  ERR_INVALID_PACKAGE_TARGET,
  ERR_INVALID_MODULE_SPECIFIER,
  ERR_PACKAGE_PATH_NOT_EXPORTED,
  ERR_REQUIRE_ESM
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');
const pendingDeprecation = getOptionValue('--pending-deprecation');

module.exports = {
  wrapSafe, Module, toRealPath, readPackageScope,
  get hasLoadedAnyUserCJSModule() { return hasLoadedAnyUserCJSModule; }
};

let asyncESM, ModuleJob, ModuleWrap, kInstantiated;

const {
  CHAR_FORWARD_SLASH,
  CHAR_BACKWARD_SLASH,
  CHAR_COLON
} = require('internal/constants');

const isWindows = process.platform === 'win32';

const relativeResolveCache = ObjectCreate(null);

let requireDepth = 0;
let statCache = null;

function enrichCJSError(err) {
  const stack = err.stack.split('\n');

  const lineWithErr = stack[1];

  /*
    The regular expression below targets the most common import statement
    usage. However, some cases are not matching, cases like import statement
    after a comment block and/or after a variable definition.
  */
  if (err.message.startsWith('Unexpected token \'export\'') ||
    (RegExpPrototypeTest(/^\s*import(?=[ {'"*])\s*(?![ (])/, lineWithErr))) {
    // Emit the warning synchronously because we are in the middle of handling
    // a SyntaxError that will throw and likely terminate the process before an
    // asynchronous warning would be emitted.
    emitWarningSync(
      'To load an ES module, set "type": "module" in the package.json or use ' +
      'the .mjs extension.'
    );
  }
}

function stat(filename) {
  filename = path.toNamespacedPath(filename);
  if (statCache !== null) {
    const result = statCache.get(filename);
    if (result !== undefined) return result;
  }
  const result = internalModuleStat(filename);
  if (statCache !== null) statCache.set(filename, result);
  return result;
}

function updateChildren(parent, child, scan) {
  const children = parent && parent.children;
  if (children && !(scan && children.includes(child)))
    children.push(child);
}

function Module(id = '', parent) {
  this.id = id;
  this.path = path.dirname(id);
  this.exports = {};
  this.parent = parent;
  updateChildren(parent, this, false);
  this.filename = null;
  this.loaded = false;
  this.children = [];
}

const builtinModules = [];
for (const [id, mod] of NativeModule.map) {
  if (mod.canBeRequiredByUsers) {
    builtinModules.push(id);
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
  '\n});'
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
  get() {
    return wrap;
  },

  set(value) {
    patched = true;
    wrap = value;
  }
});

ObjectDefineProperty(Module, 'wrapper', {
  get() {
    return wrapperProxy;
  },

  set(value) {
    patched = true;
    wrapperProxy = value;
  }
});

const debug = require('internal/util/debuglog').debuglog('module');
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

  const json = internalModuleReadJSON(path.toNamespacedPath(jsonPath));
  if (json === undefined) {
    packageJsonCache.set(jsonPath, false);
    return false;
  }

  if (manifest) {
    const jsonURL = pathToFileURL(jsonPath);
    manifest.assertIntegrity(jsonURL, json);
  }

  try {
    const parsed = JSONParse(json);
    const filtered = {
      name: parsed.name,
      main: parsed.main,
      exports: parsed.exports,
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
  const rootSeparatorIndex = checkPath.indexOf(path.sep);
  let separatorIndex;
  while (
    (separatorIndex = checkPath.lastIndexOf(path.sep)) > rootSeparatorIndex
  ) {
    checkPath = checkPath.slice(0, separatorIndex);
    if (checkPath.endsWith(path.sep + 'node_modules'))
      return false;
    const pjson = readPackage(checkPath);
    if (pjson) return {
      path: checkPath,
      data: pjson
    };
  }
  return false;
}

function readPackageMain(requestPath) {
  const pkg = readPackage(requestPath);
  return pkg ? pkg.main : undefined;
}

function readPackageExports(requestPath) {
  const pkg = readPackage(requestPath);
  return pkg ? pkg.exports : undefined;
}

function tryPackage(requestPath, exts, isMain, originalPath) {
  const pkg = readPackageMain(requestPath);

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
    } else if (pendingDeprecation) {
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
const realpathCache = new Map();

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
  while ((index = name.indexOf('.', startIndex)) !== -1) {
    startIndex = index + 1;
    if (index === 0) continue; // Skip dotfiles like .gitignore
    currentExtension = name.slice(index);
    if (Module._extensions[currentExtension]) return currentExtension;
  }
  return '.js';
}

function trySelf(parentPath, request) {
  const { data: pkg, path: basePath } = readPackageScope(parentPath) || {};
  if (!pkg || pkg.exports === undefined) return false;
  if (typeof pkg.name !== 'string') return false;

  let expansion;
  if (request === pkg.name) {
    expansion = '';
  } else if (StringPrototypeStartsWith(request, `${pkg.name}/`)) {
    expansion = StringPrototypeSlice(request, pkg.name.length);
  } else {
    return false;
  }

  const fromExports = applyExports(basePath, expansion);
  if (fromExports) {
    return tryFile(fromExports, false);
  }
  assert(fromExports !== false);
}

function isConditionalDotExportSugar(exports, basePath) {
  if (typeof exports === 'string')
    return true;
  if (ArrayIsArray(exports))
    return true;
  if (typeof exports !== 'object')
    return false;
  let isConditional = false;
  let firstCheck = true;
  for (const key of ObjectKeys(exports)) {
    const curIsConditional = key[0] !== '.';
    if (firstCheck) {
      firstCheck = false;
      isConditional = curIsConditional;
    } else if (isConditional !== curIsConditional) {
      throw new ERR_INVALID_PACKAGE_CONFIG(basePath, '"exports" cannot ' +
          'contain some keys starting with \'.\' and some not. The exports ' +
          'object must either be an object of package subpath keys or an ' +
          'object of main entry condition name keys only.');
    }
  }
  return isConditional;
}

function applyExports(basePath, expansion) {
  const mappingKey = `.${expansion}`;

  let pkgExports = readPackageExports(basePath);
  if (pkgExports === undefined || pkgExports === null)
    return false;

  if (isConditionalDotExportSugar(pkgExports, basePath))
    pkgExports = { '.': pkgExports };

  if (typeof pkgExports === 'object') {
    if (ObjectPrototypeHasOwnProperty(pkgExports, mappingKey)) {
      const mapping = pkgExports[mappingKey];
      return resolveExportsTarget(pathToFileURL(basePath + '/'), mapping, '',
                                  mappingKey);
    }

    let dirMatch = '';
    for (const candidateKey of ObjectKeys(pkgExports)) {
      if (candidateKey[candidateKey.length - 1] !== '/') continue;
      if (candidateKey.length > dirMatch.length &&
          StringPrototypeStartsWith(mappingKey, candidateKey)) {
        dirMatch = candidateKey;
      }
    }

    if (dirMatch !== '') {
      const mapping = pkgExports[dirMatch];
      const subpath = StringPrototypeSlice(mappingKey, dirMatch.length);
      const resolved = resolveExportsTarget(pathToFileURL(basePath + '/'),
                                            mapping, subpath, mappingKey);
      // Extension searching for folder exports only
      const rc = stat(resolved);
      if (rc === 0) return resolved;
      if (!(RegExpPrototypeTest(trailingSlashRegex, resolved))) {
        const exts = ObjectKeys(Module._extensions);
        const filename = tryExtensions(resolved, exts, false);
        if (filename) return filename;
      }
      if (rc === 1) {
        const exts = ObjectKeys(Module._extensions);
        const filename = tryPackage(resolved, exts, false,
                                    basePath + expansion);
        if (filename) return filename;
      }
      // Undefined means not found
      return;
    }
  }

  throw new ERR_PACKAGE_PATH_NOT_EXPORTED(basePath, mappingKey);
}

// This only applies to requests of a specific form:
// 1. name/.*
// 2. @scope/name/.*
const EXPORTS_PATTERN = /^((?:@[^/\\%]+\/)?[^./\\%][^/\\%]*)(\/.*)?$/;
function resolveExports(nmPath, request) {
  // The implementation's behavior is meant to mirror resolution in ESM.
  const [, name, expansion = ''] =
    StringPrototypeMatch(request, EXPORTS_PATTERN) || [];
  if (!name) {
    return false;
  }

  const basePath = path.resolve(nmPath, name);
  const fromExports = applyExports(basePath, expansion);
  if (fromExports) {
    return tryFile(fromExports, false);
  }
  return fromExports;
}

function isArrayIndex(p) {
  assert(typeof p === 'string');
  const n = Number(p);
  if (String(n) !== p)
    return false;
  if (ObjectIs(n, +0))
    return true;
  if (!Number.isInteger(n))
    return false;
  return n >= 0 && n < (2 ** 32) - 1;
}

function resolveExportsTarget(baseUrl, target, subpath, mappingKey) {
  if (typeof target === 'string') {
    let resolvedTarget, resolvedTargetPath;
    const pkgPathPath = baseUrl.pathname;
    if (StringPrototypeStartsWith(target, './')) {
      resolvedTarget = new URL(target, baseUrl);
      resolvedTargetPath = resolvedTarget.pathname;
      if (!StringPrototypeStartsWith(resolvedTargetPath, pkgPathPath) ||
          StringPrototypeIndexOf(resolvedTargetPath, '/node_modules/',
                                 pkgPathPath.length - 1) !== -1)
        resolvedTarget = undefined;
    }
    if (subpath.length > 0 && target[target.length - 1] !== '/')
      resolvedTarget = undefined;
    if (resolvedTarget === undefined)
      throw new ERR_INVALID_PACKAGE_TARGET(StringPrototypeSlice(baseUrl.pathname
        , 0, -1), mappingKey, subpath, target);
    const resolved = new URL(subpath, resolvedTarget);
    const resolvedPath = resolved.pathname;
    if (StringPrototypeStartsWith(resolvedPath, resolvedTargetPath) &&
        StringPrototypeIndexOf(resolvedPath, '/node_modules/',
                               pkgPathPath.length - 1) === -1) {
      return fileURLToPath(resolved);
    }
    throw new ERR_INVALID_MODULE_SPECIFIER(StringPrototypeSlice(baseUrl.pathname
      , 0, -1), mappingKey);
  } else if (ArrayIsArray(target)) {
    if (target.length === 0)
      throw new ERR_INVALID_PACKAGE_TARGET(StringPrototypeSlice(baseUrl.pathname
        , 0, -1), mappingKey, subpath, target);
    for (const targetValue of target) {
      try {
        return resolveExportsTarget(baseUrl, targetValue, subpath, mappingKey);
      } catch (e) {
        if (e.code !== 'ERR_PACKAGE_PATH_NOT_EXPORTED' &&
            e.code !== 'ERR_INVALID_PACKAGE_TARGET')
          throw e;
      }
    }
    // Throw last fallback error
    resolveExportsTarget(baseUrl, target[target.length - 1], subpath,
                         mappingKey);
    assert(false);
  } else if (typeof target === 'object' && target !== null) {
    const keys = ObjectKeys(target);
    if (keys.some(isArrayIndex)) {
      throw new ERR_INVALID_PACKAGE_CONFIG(baseUrl, '"exports" cannot ' +
          'contain numeric property keys.');
    }
    for (const p of keys) {
      switch (p) {
        case 'node':
        case 'require':
          try {
            return resolveExportsTarget(baseUrl, target[p], subpath,
                                        mappingKey);
          } catch (e) {
            if (e.code !== 'ERR_PACKAGE_PATH_NOT_EXPORTED') throw e;
          }
          break;
        case 'default':
          try {
            return resolveExportsTarget(baseUrl, target.default, subpath,
                                        mappingKey);
          } catch (e) {
            if (e.code !== 'ERR_PACKAGE_PATH_NOT_EXPORTED') throw e;
          }
      }
    }
    throw new ERR_PACKAGE_PATH_NOT_EXPORTED(
      StringPrototypeSlice(baseUrl.pathname, 0, -1), mappingKey + subpath);
  }
  throw new ERR_INVALID_PACKAGE_TARGET(
    StringPrototypeSlice(baseUrl.pathname, 0, -1), mappingKey, subpath, target);
}

const trailingSlashRegex = /(?:^|\/)\.?\.$/;
Module._findPath = function(request, paths, isMain) {
  const absoluteRequest = path.isAbsolute(request);
  if (absoluteRequest) {
    paths = [''];
  } else if (!paths || paths.length === 0) {
    return false;
  }

  const cacheKey = request + '\x00' +
                (paths.length === 1 ? paths[0] : paths.join('\x00'));
  const entry = Module._pathCache[cacheKey];
  if (entry)
    return entry;

  let exts;
  let trailingSlash = request.length > 0 &&
    request.charCodeAt(request.length - 1) === CHAR_FORWARD_SLASH;
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
      // Undefined means not found, false means no exports
      if (exportsResolved === undefined)
        break;
      if (exportsResolved) {
        return exportsResolved;
      }
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
    if (from.charCodeAt(from.length - 1) === CHAR_BACKWARD_SLASH &&
        from.charCodeAt(from.length - 2) === CHAR_COLON)
      return [from + 'node_modules'];

    const paths = [];
    for (let i = from.length - 1, p = 0, last = from.length; i >= 0; --i) {
      const code = from.charCodeAt(i);
      // The path segment separator check ('\' and '/') was used to get
      // node_modules path for every path segment.
      // Use colon as an extra condition since we can get node_modules
      // path for drive root like 'C:\node_modules' and don't need to
      // parse drive name.
      if (code === CHAR_BACKWARD_SLASH ||
          code === CHAR_FORWARD_SLASH ||
          code === CHAR_COLON) {
        if (p !== nmLen)
          paths.push(from.slice(0, last) + '\\node_modules');
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
      const code = from.charCodeAt(i);
      if (code === CHAR_FORWARD_SLASH) {
        if (p !== nmLen)
          paths.push(from.slice(0, last) + '/node_modules');
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
    paths.push('/node_modules');

    return paths;
  };
}

Module._resolveLookupPaths = function(request, parent) {
  if (NativeModule.canBeRequiredByUsers(request)) {
    debug('looking for %j in []', request);
    return null;
  }

  // Check for node modules paths.
  if (request.charAt(0) !== '.' ||
      (request.length > 1 &&
      request.charAt(1) !== '.' &&
      request.charAt(1) !== '/' &&
      (!isWindows || request.charAt(1) !== '\\'))) {

    let paths = modulePaths;
    if (parent != null && parent.paths && parent.paths.length) {
      paths = parent.paths.concat(paths);
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
// warns when non-existend properties are accessed.
const CircularRequirePrototypeWarningProxy = new Proxy({}, {
  get(target, prop) {
    if (prop in target) return target[prop];
    emitCircularRequireWarning(prop);
    return undefined;
  },

  getOwnPropertyDescriptor(target, prop) {
    if (ObjectPrototypeHasOwnProperty(target, prop))
      return ObjectGetOwnPropertyDescriptor(target, prop);
    emitCircularRequireWarning(prop);
    return undefined;
  }
});

// Object.prototype and ObjectProtoype refer to our 'primordials' versions
// and are not identical to the versions on the global object.
const PublicObjectPrototype = global.Object.prototype;

function getExportsForCircularRequire(module) {
  if (module.exports &&
      ObjectGetPrototypeOf(module.exports) === PublicObjectPrototype &&
      // Exclude transpiled ES6 modules / TypeScript code because those may
      // employ unusual patterns for accessing 'module.exports'. That should be
      // okay because ES6 modules have a different approach to circular
      // dependencies anyway.
      !module.exports.__esModule) {
    // This is later unset once the module is done loading.
    ObjectSetPrototypeOf(module.exports, CircularRequirePrototypeWarningProxy);
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

  const cachedModule = Module._cache[filename];
  if (cachedModule !== undefined) {
    updateChildren(parent, cachedModule, true);
    if (!cachedModule.loaded)
      return getExportsForCircularRequire(cachedModule);
    return cachedModule.exports;
  }

  const mod = loadNativeModule(filename, request);
  if (mod && mod.canBeRequiredByUsers) return mod.exports;

  // Don't call updateChildren(), Module constructor already does.
  const module = new Module(filename, parent);

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
    // Intercept exceptions that occur during the first tick and rekey them
    // on error instance rather than module instance (which will immediately be
    // garbage collected).
    if (enableSourceMaps) {
      try {
        module.load(filename);
      } catch (err) {
        rekeySourceMap(Module._cache[filename], err);
        throw err; /* node-do-not-add-exception-line */
      }
    } else {
      module.load(filename);
    }
    threw = false;
  } finally {
    if (threw) {
      delete Module._cache[filename];
      if (parent !== undefined) {
        delete relativeResolveCache[relResolveCacheIdentifier];
        const children = parent && parent.children;
        if (ArrayIsArray(children)) {
          const index = children.indexOf(module);
          if (index !== -1) {
            children.splice(index, 1);
          }
        }
      }
    } else if (module.exports &&
               ObjectGetPrototypeOf(module.exports) ===
                 CircularRequirePrototypeWarningProxy) {
      ObjectSetPrototypeOf(module.exports, PublicObjectPrototype);
    }
  }

  return module.exports;
};

Module._resolveFilename = function(request, parent, isMain, options) {
  if (NativeModule.canBeRequiredByUsers(request)) {
    return request;
  }

  let paths;

  if (typeof options === 'object' && options !== null) {
    if (ArrayIsArray(options.paths)) {
      const isRelative = request.startsWith('./') ||
          request.startsWith('../') ||
          ((isWindows && request.startsWith('.\\')) ||
          request.startsWith('..\\'));

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
            if (!paths.includes(lookupPaths[j]))
              paths.push(lookupPaths[j]);
          }
        }
      }
    } else if (options.paths === undefined) {
      paths = Module._resolveLookupPaths(request, parent);
    } else {
      throw new ERR_INVALID_OPT_VALUE('options.paths', options.paths);
    }
  } else {
    paths = Module._resolveLookupPaths(request, parent);
  }

  if (parent && parent.filename) {
    const filename = trySelf(parent.filename, request);
    if (filename) {
      const cacheKey = request + '\x00' +
          (paths.length === 1 ? paths[0] : paths.join('\x00'));
      Module._pathCache[cacheKey] = filename;
      return filename;
    }
  }

  // Look up the filename first, since that's the cache key.
  const filename = Module._findPath(request, paths, isMain, false);
  if (filename) return filename;
  const requireStack = [];
  for (let cursor = parent;
    cursor;
    cursor = cursor.parent) {
    requireStack.push(cursor.filename || cursor.id);
  }
  let message = `Cannot find module '${request}'`;
  if (requireStack.length > 0) {
    message = message + '\nRequire stack:\n- ' + requireStack.join('\n- ');
  }
  // eslint-disable-next-line no-restricted-syntax
  const err = new Error(message);
  err.code = 'MODULE_NOT_FOUND';
  err.requireStack = requireStack;
  throw err;
};


// Given a file name, pass it to the proper extension handler.
Module.prototype.load = function(filename) {
  debug('load %j for module %j', filename, this.id);

  assert(!this.loaded);
  this.filename = filename;
  this.paths = Module._nodeModulePaths(path.dirname(filename));

  const extension = findLongestRegisteredExtension(filename);
  // allow .mjs to be overridden
  if (filename.endsWith('.mjs') && !Module._extensions['.mjs']) {
    throw new ERR_REQUIRE_ESM(filename);
  }
  Module._extensions[extension](this, filename);
  this.loaded = true;

  const ESMLoader = asyncESM.ESMLoader;
  const url = `${pathToFileURL(filename)}`;
  const module = ESMLoader.moduleMap.get(url);
  // Create module entry at load time to snapshot exports correctly
  const exports = this.exports;
  // Called from cjs translator
  if (module !== undefined && module.module !== undefined) {
    if (module.module.getStatus() >= kInstantiated)
      module.module.setExport('default', exports);
  } else {
    // Preemptively cache
    // We use a function to defer promise creation for async hooks.
    ESMLoader.moduleMap.set(
      url,
      // Module job creation will start promises.
      // We make it a function to lazily trigger those promises
      // for async hooks compatibility.
      () => new ModuleJob(ESMLoader, url, () =>
        new ModuleWrap(url, undefined, ['default'], function() {
          this.setExport('default', exports);
        })
      , false /* isMain */, false /* inspectBrk */)
    );
  }
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
      importModuleDynamically: async (specifier) => {
        const loader = asyncESM.ESMLoader;
        return loader.import(specifier, normalizeReferrerURL(filename));
      },
    });
  }
  let compiled;
  try {
    compiled = compileFunction(
      content,
      filename,
      0,
      0,
      undefined,
      false,
      undefined,
      [],
      [
        'exports',
        'require',
        'module',
        '__filename',
        '__dirname',
      ]
    );
  } catch (err) {
    if (process.mainModule === cjsModuleInstance)
      enrichCJSError(err);
    throw err;
  }

  const { callbackMap } = internalBinding('module_wrap');
  callbackMap.set(compiled.cacheKey, {
    importModuleDynamically: async (specifier) => {
      const loader = asyncESM.ESMLoader;
      return loader.import(specifier, normalizeReferrerURL(filename));
    }
  });

  return compiled.function;
}

// Run the file contents in the correct scope or sandbox. Expose
// the correct helper variables (require, module, exports) to
// the file.
// Returns exception, if any.
Module.prototype._compile = function(content, filename) {
  let moduleURL;
  let redirects;
  if (manifest) {
    moduleURL = pathToFileURL(filename);
    redirects = manifest.getRedirector(moduleURL);
    manifest.assertIntegrity(moduleURL, content);
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
  if (requireDepth === 0) statCache = new Map();
  if (inspectorWrapper) {
    result = inspectorWrapper(compiledWrapper, thisValue, exports,
                              require, module, filename, dirname);
  } else {
    result = compiledWrapper.call(thisValue, exports, require, module,
                                  filename, dirname);
  }
  hasLoadedAnyUserCJSModule = true;
  if (requireDepth === 0) statCache = null;
  return result;
};

// Native extension for .js
Module._extensions['.js'] = function(module, filename) {
  if (filename.endsWith('.js')) {
    const pkg = readPackageScope(filename);
    // Function require shouldn't be used in ES modules.
    if (pkg && pkg.data && pkg.data.type === 'module') {
      const parentPath = module.parent && module.parent.filename;
      const packageJsonPath = path.resolve(pkg.path, 'package.json');
      throw new ERR_REQUIRE_ESM(filename, parentPath, packageJsonPath);
    }
  }
  const content = fs.readFileSync(filename, 'utf8');
  module._compile(content, filename);
};


// Native extension for .json
Module._extensions['.json'] = function(module, filename) {
  const content = fs.readFileSync(filename, 'utf8');

  if (manifest) {
    const moduleURL = pathToFileURL(filename);
    manifest.assertIntegrity(moduleURL, content);
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
  if (manifest) {
    const content = fs.readFileSync(filename);
    const moduleURL = pathToFileURL(filename);
    manifest.assertIntegrity(moduleURL, content);
  }
  // Be aware this doesn't use `content`
  return process.dlopen(module, path.toNamespacedPath(filename));
};

function createRequireFromPath(filename) {
  // Allow a directory to be passed as the filename
  const trailingSlash =
    filename.endsWith('/') || (isWindows && filename.endsWith('\\'));

  const proxyPath = trailingSlash ?
    path.join(filename, 'noop.js') :
    filename;

  const m = new Module(proxyPath);
  m.filename = proxyPath;

  m.paths = Module._nodeModulePaths(m.path);
  return makeRequireFunction(m, null);
}

Module.createRequireFromPath = deprecate(
  createRequireFromPath,
  'Module.createRequireFromPath() is deprecated. ' +
  'Use Module.createRequire() instead.',
  'DEP0130'
);

const createRequireError = 'must be a file URL object, file URL string, or ' +
  'absolute path string';

function createRequire(filename) {
  let filepath;

  if (filename instanceof URL ||
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

  let paths = [path.resolve(prefixDir, 'lib', 'node')];

  if (homeDir) {
    paths.unshift(path.resolve(homeDir, '.node_libraries'));
    paths.unshift(path.resolve(homeDir, '.node_modules'));
  }

  if (nodePath) {
    paths = nodePath.split(path.delimiter).filter(function pathsFilterCB(path) {
      return !!path;
    }).concat(paths);
  }

  modulePaths = paths;

  // Clone as a shallow copy, for introspection.
  Module.globalPaths = modulePaths.slice(0);
};

Module._preloadModules = function(requests) {
  if (!ArrayIsArray(requests))
    return;

  // Preloaded modules have a dummy parent module which is deemed to exist
  // in the current working directory. This seeds the search path for
  // preloaded modules.
  const parent = new Module('internal/preload', null);
  try {
    parent.paths = Module._nodeModulePaths(process.cwd());
  } catch (e) {
    if (e.code !== 'ENOENT') {
      throw e;
    }
  }
  for (let n = 0; n < requests.length; n++)
    parent.require(requests[n]);
};

Module.syncBuiltinESMExports = function syncBuiltinESMExports() {
  for (const mod of NativeModule.map.values()) {
    if (mod.canBeRequiredByUsers) {
      mod.syncExports();
    }
  }
};

// Backwards compatibility
Module.Module = Module;

// We have to load the esm things after module.exports!
asyncESM = require('internal/process/esm_loader');
ModuleJob = require('internal/modules/esm/module_job');
({ ModuleWrap, kInstantiated } = internalBinding('module_wrap'));
