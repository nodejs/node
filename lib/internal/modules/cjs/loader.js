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

const { NativeModule } = require('internal/bootstrap/loaders');
const util = require('util');
const { pathToFileURL } = require('internal/url');
const vm = require('vm');
const assert = require('assert').ok;
const fs = require('fs');
const internalFS = require('internal/fs/utils');
const path = require('path');
const { URL } = require('url');
const {
  internalModuleReadJSON,
  internalModuleStat
} = internalBinding('fs');
const { safeGetenv } = internalBinding('credentials');
const {
  makeRequireFunction,
  stripBOM,
  stripShebang
} = require('internal/modules/cjs/helpers');
const {
  decorateErrorStack
} = require('internal/util');
const { getOptionValue } = require('internal/options');
const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const experimentalModules = getOptionValue('--experimental-modules');

const {
  ERR_INVALID_ARG_VALUE,
  ERR_REQUIRE_ESM
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');

let asyncESM;
let ModuleJob;
let createDynamicModule;
let extensionNames;
let extensionCopy;
let getNodeModulePaths;

const {
  CHAR_FORWARD_SLASH,
  CHAR_BACKWARD_SLASH,
  CHAR_COLON,
  CHAR_DOT,
} = require('internal/constants');

const isWindows = process.platform === 'win32';

const statCache = new Map();
function stat(filename) {
  filename = path.toNamespacedPath(filename);
  let result = statCache.get(filename);
  if (result !== undefined)
    return result;
  result = internalModuleStat(filename);
  if (statCache.size < 1e4)
    statCache.set(filename, result);
  return result;
}

function updateChildren(parent, child) {
  const children = parent.children;
  if (children.indexOf(child) === -1)
    children.push(child);
}

function Module(id, parent) {
  this.id = id;
  this.path = path.dirname(id);
  this.exports = {};
  this.parent = parent;
  this.filename = null;
  this.loaded = false;
  this.children = [];
  this.paths = undefined;
  if (parent !== undefined)
    parent.children.push(this);
}

const builtinModules = Object.keys(NativeModule._source)
  .filter(NativeModule.nonInternalExists);

Object.freeze(builtinModules);
Module.builtinModules = builtinModules;

Module._cache = Object.create(null);
Module._pathCache = Object.create(null);
let modulePaths = [];
Module.globalPaths = [];

Module.wrap = function(script) {
  return Module.wrapper[0] + script + Module.wrapper[1];
};

Module.wrapper = [
  '(function (exports, require, module, __filename, __dirname) { ',
  '\n});'
];

const debug = util.debuglog('module');

Module._debug = util.deprecate(debug, 'Module._debug is deprecated.',
                               'DEP0077');

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

// Check if the directory is a package.json dir
function readPackage(requestPath) {
  const jsonPath = `${requestPath}${path.sep}package.json`;
  const json = internalModuleReadJSON(path.toNamespacedPath(jsonPath));

  if (json === undefined) {
    return '';
  }

  try {
    return JSON.parse(json).main;
  } catch (e) {
    e.path = jsonPath;
    e.message = `Error parsing ${jsonPath}: ${e.message}`;
    throw e;
  }
}

function tryPackage(requestPath, isMain) {
  const pkg = readPackage(requestPath);

  if (!pkg) return '';

  const filename = path.resolve(requestPath, pkg);
  return tryFile(filename, isMain) ||
         tryExtensions(filename, isMain) ||
         tryExtensions(`${filename}${path.sep}index`, isMain);
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
  if (rc !== 0) {
    return '';
  }
  if (preserveSymlinks && !isMain) {
    return requestPath;
  }
  return toRealPath(requestPath);
}

function toRealPath(requestPath) {
  return fs.realpathSync(requestPath, {
    [internalFS.realpathCacheKey]: realpathCache
  });
}

// Given a path, check if the file exists with any of the set extensions.
function tryExtensions(p, isMain) {
  for (var i = 0; i < extensionNames.length; i++) {
    const filename = tryFile(`${p}${extensionNames[i]}`, isMain);

    if (filename !== '') {
      return filename;
    }
  }
  return '';
}

// Find the longest (possibly multi-dot) extension registered in
// Module._extensions
function findLongestRegisteredExtension(filename) {
  const name = path.basename(filename);
  let currentExtension;
  let index = 0;
  do {
    index = name.indexOf('.', index);
    currentExtension = name.slice(index);
    if (extensionCopy[currentExtension]) {
      if (index === 0)
        continue; // Skip dotfiles like .gitignore
      return currentExtension;
    }
  } while (index++ !== -1);
  return '.js';
}

function hasTrailingSlash(request) {
  let char = request.charCodeAt(request.length - 1);
  if (char === CHAR_FORWARD_SLASH) {
    return true;
  }
  if (char !== CHAR_DOT) {
    return false;
  }
  if (request.length === 1) {
    return true;
  }
  char = request.charCodeAt(request.length - 2);
  if (char === CHAR_DOT) {
    return request.length === 2 ||
      request.charCodeAt(request.length - 3) === CHAR_FORWARD_SLASH;
  }
  return char === CHAR_FORWARD_SLASH;
}

Module._findPath = function(request, paths, isMain) {
  let trailingSlash = false;

  // For each path
  for (var i = 0; i < paths.length; i++) {
    // Don't search further if path doesn't exist
    const curPath = paths[i];
    const cacheKey = `${request}\x00${curPath}`;
    const cache = Module._pathCache[cacheKey];
    if (cache !== undefined) {
      if (cache === '') // Path does not contain anything.
        continue;
      return cache;
    }
    if (i === 0)
      trailingSlash = hasTrailingSlash(request);

    const basePath = curPath !== '' ?
      path.resolve(curPath, request) :
      path.resolve(request);

    let filename = '';

    const rc = stat(basePath);
    if (rc !== -1 && !trailingSlash) {
      if (rc === 0) {  // File.
        if (!isMain && preserveSymlinks || preserveSymlinksMain) {
          // For the main module, we use the preserveSymlinksMain flag instead
          // mainly for backward compatibility, as the preserveSymlinks flag
          // historically has not applied to the main module.  Most likely this
          // was intended to keep .bin/ binaries working, as following those
          // symlinks is usually required for the imports in the corresponding
          // files to resolve; that said, in some use cases following symlinks
          // causes bigger problems which is why the preserveSymlinksMain option
          // is needed.
          filename = basePath;
        } else {
          filename = toRealPath(basePath);
        }
      } else {
        // Try it with each of the extensions
        filename = tryExtensions(basePath, isMain);
      }
    }

    if (rc === 1 && filename === '') {  // Directory.
      // Try it with each of the extensions at "index"
      filename = tryPackage(basePath, isMain);
      if (filename === '') {
        filename = tryExtensions(`${basePath}${path.sep}index`, isMain);
      }
    }

    if (filename !== '') {
      Module._pathCache[cacheKey] = filename;
      return filename;
    }
    // Signal that we already checked this path and that it did not contain
    // a file.
    Module._pathCache[cacheKey] = '';
  }
  // eslint-disable-next-line no-restricted-syntax
  const err = new Error(`Cannot find module '${request}'`);
  err.code = 'MODULE_NOT_FOUND';
  throw err;
};

// This wrapper exists for backwards compatibility. It just guarantees that
// `from` is absolute.
Module._nodeModulePaths = function(from) {
  return getNodeModulePaths(path.resolve(from));
};

// 'node_modules' character codes reversed
const nmChars = [ 115, 101, 108, 117, 100, 111, 109, 95, 101, 100, 111, 110 ];
const nmLen = nmChars.length;
if (isWindows) {
  // 'from' is the __dirname of the module.
  getNodeModulePaths = (from) => {
    // Note: this approach *only* works when the path is guaranteed
    // to be absolute.  Doing a fully-edge-case-correct path.split
    // that works on both Windows and Posix is non-trivial.

    // Return root node_modules when path is 'D:\\'.
    // path.resolve will make sure from.length >=3 in Windows.
    if (from.charCodeAt(from.length - 1) === CHAR_BACKWARD_SLASH &&
        from.charCodeAt(from.length - 2) === CHAR_COLON) {
      return [`${from}node_modules`];
    }

    const paths = [];
    let p = 0;
    let last = from.length;
    for (var i = from.length - 1; i >= 0; --i) {
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
          paths.push(`${from.slice(0, last)}\\node_modules`);
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
} else { // Posix
  getNodeModulePaths = (from) => {
    // TODO(BridgeAR): Consider checking for the directory existence and to only
    // return existing ones + adding a local cache to prevent calculating the
    // path again for the same directory. This would slow down this function
    // while decreasing the amount of `_pathCache` entries.

    // Return early not only to avoid unnecessary work, but to *avoid* returning
    // an array of two items for a root: [ '//node_modules', '/node_modules' ]
    if (from.length === 1)
      return ['/node_modules'];

    // Note: this approach *only* works when the path is guaranteed
    // to be absolute.  Doing a fully-edge-case-correct path.split
    // that works on both Windows and Posix is non-trivial.
    const paths = [];
    let p = 0;
    let last = from.length;
    for (var i = from.length - 1; i >= 0; --i) {
      const code = from.charCodeAt(i);
      if (code === CHAR_FORWARD_SLASH) {
        if (p !== nmLen)
          paths.push(`${from.slice(0, last)}/node_modules`);
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
  if (path.isAbsolute(request)) {
    debug('looking for absolute path "%s"', request);
    return [''];
  }

  // Check for node modules paths.
  if (request.charAt(0) !== '.' ||
      (request.length > 1 &&
       request.charAt(1) !== '.' &&
       request.charAt(1) !== '/' &&
       (!isWindows || request.charAt(1) !== '\\'))) {
    let paths = modulePaths;
    if (parent != null && parent.paths.length) {
      paths = parent.paths.concat(paths);
    }

    debug('looking for "%s" in %j', request, paths);
    return paths;
  }

  // With --eval, parent.id is not set and parent.filename is null.
  if (!parent || !parent.id || !parent.filename) {
    // Make require('./path/to/foo') work - normally the path is taken
    // from realpath(__filename) but with eval there is no filename
    const mainPaths = ['.'].concat(
      getNodeModulePaths(process.cwd()),
      modulePaths
    );

    debug('looking for "%s" in %j', request, mainPaths);
    return mainPaths;
  }

  return [parent.path];
};

const filenameCache = Object.create(null);

// Check the cache for the requested file.
// 1. If a module already exists in the cache: return its exports object.
// 2. If the module is native: call `NativeModule.require()` with the
//    filename and return the result.
// 3. Otherwise, create a new module for the file and save it to the cache.
//    Then have it load  the file contents before returning its exports
//    object.
Module._load = function(request, parent, isMain) {
  let identifier;
  if (parent !== undefined) {
    debug('Module._load REQUEST %s parent: %s', request, parent.id);

    // Fast path for (lazy loaded) modules in the same directory. The indirect
    // caching is required to allow cache invalidation without changing the old
    // cache key names.
    identifier = `${parent.path}\x00${request}`;
    const filename = filenameCache[identifier];
    if (filename !== undefined) {
      const cachedModule = Module._cache[filename];
      if (cachedModule !== undefined) {
        updateChildren(parent, cachedModule, true);
        return cachedModule.exports;
      }
      delete filenameCache[identifier];
    }
  }

  // Check for native modules first. If we find such name in the user module
  // cache, bail out. This keeps the behavior backwards compatible. It was
  // originally meant to allow mocking of whole modules by placing a native
  // module name in there.
  if (NativeModule.nonInternalExists(request) &&
      Module._cache[request] === undefined) {
    debug('load native module %s', request);
    return NativeModule.require(request);
  }

  const filename = Module._resolveFilename(request, parent, isMain);

  const cachedModule = Module._cache[filename];
  if (cachedModule !== undefined) {
    if (parent !== undefined)
      updateChildren(parent, cachedModule, true);
    return cachedModule.exports;
  }

  // Don't call updateChildren(), Module constructor already does.
  const module = new Module(filename, parent);

  if (isMain) {
    process.mainModule = module;
    module.id = '.';
  }

  Module._cache[filename] = module;
  if (parent !== undefined)
    filenameCache[identifier] = filename;

  let threw = true;
  try {
    module.load(filename);
    threw = false;
  } finally {
    if (threw) {
      delete Module._cache[filename];
      if (parent !== undefined)
        delete filenameCache[identifier];
    }
  }

  return module.exports;
};

Module._resolveFilename = function(request, parent, isMain, paths) {
  if (NativeModule.nonInternalExists(request)) {
    return request;
  }

  if (paths === undefined)
    paths = Module._resolveLookupPaths(request, parent);

  return Module._findPath(request, paths, isMain);
};

// Given a file name, pass it to the proper extension handler.
Module.prototype.load = function(filename) {
  debug('load "%s" for module "%s"', filename, this.id);

  assert(!this.loaded);
  this.filename = filename;
  this.paths = getNodeModulePaths(this.path);

  const extension = findLongestRegisteredExtension(filename);
  Module._extensions[extension](this, filename);
  this.loaded = true;

  if (experimentalModules) {
    const ESMLoader = asyncESM.ESMLoader;
    const url = `${pathToFileURL(filename)}`;
    const module = ESMLoader.moduleMap.get(url);
    // Create module entry at load time to snapshot exports correctly
    const exports = this.exports;
    if (module !== undefined) { // Called from cjs translator
      module.reflect.onReady((reflect) => {
        reflect.exports.default.set(exports);
      });
    } else { // Preemptively cache
      ESMLoader.moduleMap.set(
        url,
        new ModuleJob(ESMLoader, url, async () => {
          return createDynamicModule(
            ['default'], url, (reflect) => {
              reflect.exports.default.set(exports);
            });
        })
      );
    }
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
  return Module._load(id, this, /* isMain */ false);
};

// Resolved path to process.argv[1] will be lazily placed here
// (needed for setting breakpoint when called with --inspect-brk)
let resolvedArgv;

function normalizeReferrerURL(referrer) {
  if (typeof referrer === 'string' && path.isAbsolute(referrer)) {
    return pathToFileURL(referrer).href;
  }
  return new URL(referrer).href;
}

// Run the file contents in the correct scope or sandbox. Expose
// the correct helper variables (require, module, exports) to
// the file.
// Returns exception, if any.
Module.prototype._compile = function(content, filename) {
  content = stripShebang(content);

  // Create wrapper function
  const wrapper = Module.wrap(content);

  const compiledWrapper = vm.runInThisContext(wrapper, {
    filename: filename,
    lineOffset: 0,
    displayErrors: true,
    importModuleDynamically: experimentalModules ? async (specifier) => {
      const loader = await asyncESM.loaderPromise;
      return loader.import(specifier, normalizeReferrerURL(filename));
    } : undefined,
  });

  const require = makeRequireFunction(this);

  if (process._breakFirstLine && process._eval == null && !resolvedArgv) {
    // We enter the repl if we're not given a filename argument.
    if (process.argv[1]) {
      resolvedArgv = Module._resolveFilename(process.argv[1], null, false);
    } else {
      resolvedArgv = 'repl';
    }
    // Set breakpoint on module start
    if (filename === resolvedArgv) {
      delete process._breakFirstLine;
      const inspectorWrapper = internalBinding('inspector').callAndPauseOnStart;
      return inspectorWrapper(compiledWrapper, this.exports, this.exports,
                              require, this, filename, this.path);
    }
  }

  return compiledWrapper.call(this.exports, this.exports, require, this,
                              filename, this.path);
};

// Bootstrap main module.
Module.runMain = function() {
  // Load the main module--the command line argument.
  if (experimentalModules) {
    asyncESM.loaderPromise.then((loader) => {
      return loader.import(pathToFileURL(process.argv[1]).pathname);
    })
    .catch((e) => {
      decorateErrorStack(e);
      console.error(e);
      process.exit(1);
    });
  } else {
    Module._load(process.argv[1], undefined, true);
  }
  // Handle any nextTicks added in the first tick of the program
  process._tickCallback();
};

Module.createRequireFromPath = (filename) => {
  const m = new Module(filename);
  m.filename = filename;
  m.paths = getNodeModulePaths(m.path);
  return makeRequireFunction(m);
};

Module._initPaths = function() {
  let homeDir;
  let nodePath;
  // $PREFIX/lib/node, where $PREFIX is the root of the Node.js installation.
  // `process.execPath` is $PREFIX/bin/node except on Windows where it is
  // $PREFIX\node.exe.
  let prefixDir;
  if (isWindows) {
    homeDir = process.env.USERPROFILE;
    nodePath = process.env.NODE_PATH;
    prefixDir = path.resolve(process.execPath, '..');
  } else {
    homeDir = safeGetenv('HOME');
    nodePath = safeGetenv('NODE_PATH');
    prefixDir = path.resolve(process.execPath, '..', '..');
  }

  let paths = [path.resolve(prefixDir, 'lib', 'node')];

  if (homeDir) {
    paths.unshift(path.resolve(homeDir, '.node_libraries'));
    paths.unshift(path.resolve(homeDir, '.node_modules'));
  }

  if (nodePath) {
    paths = nodePath.split(path.delimiter)
                    .filter((path) => !!path)
                    .concat(paths);
  }

  // Filter all paths that do not exist or that resolve to a file instead of a
  // directory.
  modulePaths = paths.filter((path) => stat(path) === 1);

  // Use the original input for introspection.
  Module.globalPaths = paths;
};

Module._preloadModules = function(requests) {
  // Preloaded modules have a dummy parent module which is deemed to exist
  // in the current working directory. This seeds the search path for
  // preloaded modules.
  const parent = new Module('internal/preload');
  try {
    parent.paths = getNodeModulePaths(process.cwd());
  } catch (e) {
    if (e.code !== 'ENOENT') {
      throw e;
    }
    parent.paths = [];
  }
  for (const request of requests) {
    parent.require(request);
  }
};

Module._initPaths();

// Backwards compatibility
Module.Module = Module;

const extensions = Object.create(null);

// Native extension for .js
extensions['.js'] = function(module, filename) {
  const content = fs.readFileSync(filename, 'utf8');
  module._compile(stripBOM(content), filename);
};

// Native extension for .json
extensions['.json'] = function(module, filename) {
  const content = fs.readFileSync(filename, 'utf8');
  try {
    module.exports = JSON.parse(stripBOM(content));
  } catch (err) {
    err.message = `${filename}: ${err.message}`;
    throw err;
  }
};

// Native extension for .node
extensions['.node'] = function(module, filename) {
  return process.dlopen(module, path.toNamespacedPath(filename));
};

if (experimentalModules) {
  extensions['.mjs'] = function(module, filename) {
    throw new ERR_REQUIRE_ESM(filename);
  };
}

// Use proxies to optimize resetting the extensions. This would otherwise be a
// significant overhead as we have to check for all object keys each time a
// uncached module is loaded (everything else would be a breaking change).
function setExtensionsProxy(extensions) {
  // Intersect manipulating the extensions.
  extensionNames = Object.keys(extensions);
  extensionCopy = { ...extensions };
  return new Proxy(extensions, {
    set(obj, prop, value) {
      if (extensions[prop] === undefined) {
        extensionCopy[prop] = value;
        extensionNames.push(prop);
      }
      return Reflect.set(obj, prop, value);
    },
    defineProperty(target, prop, descriptor) {
      if (extensions[prop] === undefined) {
        Object.defineProperty(extensionCopy, prop, descriptor);
        extensionNames.push(prop);
      }
      return Reflect.defineProperty(target, prop, descriptor);
    },
    deleteProperty(target, prop) {
      const pos = extensionNames.indexOf(prop);
      if (pos !== -1) {
        delete extensionCopy[prop];
        extensionNames.splice(pos, 1);
      }
      return Reflect.deleteProperty(target, prop);
    }
  });
}

// Intersect manipulating the extensions.
const ProxyModule = new Proxy(Module, {
  set(obj, prop, value) {
    if (prop === '_extensions') {
      value = setExtensionsProxy(value);
    }
    return Reflect.set(obj, prop, value);
  },
  defineProperty(target, prop, descriptor) {
    if (prop === '_extensions') {
      if (descriptor.value) {
        descriptor = {
          ...descriptor,
          value: setExtensionsProxy(descriptor.value)
        };
      } else {
        descriptor = {
          ...descriptor,
          get() {
            return setExtensionsProxy(descriptor.get());
          }
        };
      }
    }
    return Reflect.defineProperty(target, prop, descriptor);
  }
});

Module._extensions = setExtensionsProxy(extensions);

module.exports = ProxyModule;

// We have to load the esm things after module.exports!
if (experimentalModules) {
  asyncESM = require('internal/process/esm_loader');
  ModuleJob = require('internal/modules/esm/module_job');
  createDynamicModule = require(
    'internal/modules/esm/create_dynamic_module');
}
