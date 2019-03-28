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
const { pathToFileURL } = require('internal/url');
const { deprecate } = require('internal/util');
const vm = require('vm');
const assert = require('internal/assert');
const fs = require('fs');
const internalFS = require('internal/fs/utils');
const path = require('path');
const {
  internalModuleReadJSON,
  internalModuleStat
} = internalBinding('fs');
const { safeGetenv } = internalBinding('credentials');
const {
  makeRequireFunction,
  normalizeReferrerURL,
  stripBOM,
  stripShebang
} = require('internal/modules/cjs/helpers');
const { getOptionValue } = require('internal/options');
const preserveSymlinks = getOptionValue('--preserve-symlinks');
const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
const experimentalModules = getOptionValue('--experimental-modules');
const manifest = getOptionValue('--experimental-policy') ?
  require('internal/process/policy').manifest :
  null;
const { compileFunction } = internalBinding('contextify');

const {
  ERR_INVALID_ARG_VALUE,
  ERR_REQUIRE_ESM
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');

module.exports = Module;

let asyncESM;
let ModuleJob;
let createDynamicModule;

const {
  CHAR_UPPERCASE_A,
  CHAR_LOWERCASE_A,
  CHAR_UPPERCASE_Z,
  CHAR_LOWERCASE_Z,
  CHAR_FORWARD_SLASH,
  CHAR_BACKWARD_SLASH,
  CHAR_COLON,
  CHAR_DOT,
  CHAR_UNDERSCORE,
  CHAR_0,
  CHAR_9,
} = require('internal/constants');

const isWindows = process.platform === 'win32';

const relativeResolveCache = Object.create(null);

let requireDepth = 0;
let statCache = null;
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

Object.freeze(builtinModules);
Module.builtinModules = builtinModules;

Module._cache = Object.create(null);
Module._pathCache = Object.create(null);
Module._extensions = Object.create(null);
var modulePaths = [];
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
    return Reflect.set(target, property, value, receiver);
  },

  defineProperty(target, property, descriptor) {
    patched = true;
    return Object.defineProperty(target, property, descriptor);
  }
});

Object.defineProperty(Module, 'wrap', {
  get() {
    return wrap;
  },

  set(value) {
    patched = true;
    wrap = value;
  }
});

Object.defineProperty(Module, 'wrapper', {
  get() {
    return wrapperProxy;
  },

  set(value) {
    patched = true;
    wrapperProxy = value;
  }
});

let debuglog;
function debug(...args) {
  if (!debuglog) {
    debuglog = require('internal/util/debuglog').debuglog('module');
  }
  debuglog(...args);
}

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

// Check if the directory is a package.json dir.
const packageMainCache = Object.create(null);

function readPackage(requestPath) {
  const entry = packageMainCache[requestPath];
  if (entry)
    return entry;

  const jsonPath = path.resolve(requestPath, 'package.json');
  const json = internalModuleReadJSON(path.toNamespacedPath(jsonPath));

  if (json === undefined) {
    return false;
  }

  if (manifest) {
    const jsonURL = pathToFileURL(jsonPath);
    manifest.assertIntegrity(jsonURL, json);
  }

  try {
    return packageMainCache[requestPath] = JSON.parse(json).main;
  } catch (e) {
    e.path = jsonPath;
    e.message = 'Error parsing ' + jsonPath + ': ' + e.message;
    throw e;
  }
}

function tryPackage(requestPath, exts, isMain) {
  var pkg = readPackage(requestPath);

  if (!pkg) return false;

  var filename = path.resolve(requestPath, pkg);
  return tryFile(filename, isMain) ||
         tryExtensions(filename, exts, isMain) ||
         tryExtensions(path.resolve(filename, 'index'), exts, isMain);
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
  if (preserveSymlinks && !isMain) {
    return rc === 0 && path.resolve(requestPath);
  }
  return rc === 0 && toRealPath(requestPath);
}

function toRealPath(requestPath) {
  return fs.realpathSync(requestPath, {
    [internalFS.realpathCacheKey]: realpathCache
  });
}

// Given a path, check if the file exists with any of the set extensions
function tryExtensions(p, exts, isMain) {
  for (var i = 0; i < exts.length; i++) {
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

var warned = false;
Module._findPath = function(request, paths, isMain) {
  if (path.isAbsolute(request)) {
    paths = [''];
  } else if (!paths || paths.length === 0) {
    return false;
  }

  const cacheKey = request + '\x00' +
                (paths.length === 1 ? paths[0] : paths.join('\x00'));
  const entry = Module._pathCache[cacheKey];
  if (entry)
    return entry;

  var exts;
  var trailingSlash = request.length > 0 &&
    request.charCodeAt(request.length - 1) === CHAR_FORWARD_SLASH;
  if (!trailingSlash) {
    trailingSlash = /(?:^|\/)\.?\.$/.test(request);
  }

  // For each path
  for (var i = 0; i < paths.length; i++) {
    // Don't search further if path doesn't exist
    const curPath = paths[i];
    if (curPath && stat(curPath) < 1) continue;
    var basePath = path.resolve(curPath, request);
    var filename;

    var rc = stat(basePath);
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
          exts = Object.keys(Module._extensions);
        filename = tryExtensions(basePath, exts, isMain);
      }
    }

    if (!filename && rc === 1) {  // Directory.
      // try it with each of the extensions at "index"
      if (exts === undefined)
        exts = Object.keys(Module._extensions);
      filename = tryPackage(basePath, exts, isMain);
      if (!filename) {
        filename = tryExtensions(path.resolve(basePath, 'index'), exts, isMain);
      }
    }

    if (filename) {
      // Warn once if '.' resolved outside the module dir
      if (request === '.' && i > 0) {
        if (!warned) {
          warned = true;
          process.emitWarning(
            'warning: require(\'.\') resolved outside the package ' +
            'directory. This functionality is deprecated and will be removed ' +
            'soon.',
            'DeprecationWarning', 'DEP0019');
        }
      }

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
    var p = 0;
    var last = from.length;
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
    var p = 0;
    var last = from.length;
    for (var i = from.length - 1; i >= 0; --i) {
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


// 'index.' character codes
const indexChars = [ 105, 110, 100, 101, 120, 46 ];
const indexLen = indexChars.length;
Module._resolveLookupPaths = function(request, parent, newReturn) {
  if (NativeModule.canBeRequiredByUsers(request)) {
    debug('looking for %j in []', request);
    return (newReturn ? null : [request, []]);
  }

  // Check for non-relative path
  if (request.length < 2 ||
      request.charCodeAt(0) !== CHAR_DOT ||
      (request.charCodeAt(1) !== CHAR_DOT &&
       request.charCodeAt(1) !== CHAR_FORWARD_SLASH &&
       (!isWindows || request.charCodeAt(1) !== CHAR_BACKWARD_SLASH))) {
    var paths = modulePaths;
    if (parent) {
      if (!parent.paths)
        paths = parent.paths = [];
      else
        paths = parent.paths.concat(paths);
    }

    // Maintain backwards compat with certain broken uses of require('.')
    // by putting the module's directory in front of the lookup paths.
    if (request === '.') {
      if (parent && parent.filename) {
        paths.unshift(path.dirname(parent.filename));
      } else {
        paths.unshift(path.resolve(request));
      }
    }

    debug('looking for %j in %j', request, paths);
    return (newReturn ? (paths.length > 0 ? paths : null) : [request, paths]);
  }

  // with --eval, parent.id is not set and parent.filename is null
  if (!parent || !parent.id || !parent.filename) {
    // Make require('./path/to/foo') work - normally the path is taken
    // from realpath(__filename) but with eval there is no filename
    var mainPaths = ['.'].concat(Module._nodeModulePaths('.'), modulePaths);

    debug('looking for %j in %j', request, mainPaths);
    return (newReturn ? mainPaths : [request, mainPaths]);
  }

  // Is the parent an index module?
  // We can assume the parent has a valid extension,
  // as it already has been accepted as a module.
  const base = path.basename(parent.filename);
  var parentIdPath;
  if (base.length > indexLen) {
    var i = 0;
    for (; i < indexLen; ++i) {
      if (indexChars[i] !== base.charCodeAt(i))
        break;
    }
    if (i === indexLen) {
      // We matched 'index.', let's validate the rest
      for (; i < base.length; ++i) {
        const code = base.charCodeAt(i);
        if (code !== CHAR_UNDERSCORE &&
            (code < CHAR_0 || code > CHAR_9) &&
            (code < CHAR_UPPERCASE_A || code > CHAR_UPPERCASE_Z) &&
            (code < CHAR_LOWERCASE_A || code > CHAR_LOWERCASE_Z))
          break;
      }
      if (i === base.length) {
        // Is an index module
        parentIdPath = parent.id;
      } else {
        // Not an index module
        parentIdPath = path.dirname(parent.id);
      }
    } else {
      // Not an index module
      parentIdPath = path.dirname(parent.id);
    }
  } else {
    // Not an index module
    parentIdPath = path.dirname(parent.id);
  }
  var id = path.resolve(parentIdPath, request);

  // Make sure require('./path') and require('path') get distinct ids, even
  // when called from the toplevel js file
  if (parentIdPath === '.' &&
      id.indexOf('/') === -1 &&
      (!isWindows || id.indexOf('\\') === -1)) {
    id = './' + id;
  }

  debug('RELATIVE: requested: %s set ID to: %s from %s', request, id,
        parent.id);

  const parentDir = [path.dirname(parent.filename)];
  debug('looking for %j in %j', id, parentDir);
  return (newReturn ? parentDir : [id, parentDir]);
};

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
        return cachedModule.exports;
      }
      delete relativeResolveCache[relResolveCacheIdentifier];
    }
  }

  const filename = Module._resolveFilename(request, parent, isMain);

  const cachedModule = Module._cache[filename];
  if (cachedModule !== undefined) {
    updateChildren(parent, cachedModule, true);
    return cachedModule.exports;
  }

  const mod = NativeModule.map.get(filename);
  if (mod && mod.canBeRequiredByUsers) {
    debug('load native module %s', request);
    return mod.compileForPublicLoader(experimentalModules);
  }

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
    module.load(filename);
    threw = false;
  } finally {
    if (threw) {
      delete Module._cache[filename];
      if (parent !== undefined) {
        delete relativeResolveCache[relResolveCacheIdentifier];
      }
    }
  }

  return module.exports;
};

Module._resolveFilename = function(request, parent, isMain, options) {
  if (NativeModule.canBeRequiredByUsers(request)) {
    return request;
  }

  var paths;

  if (typeof options === 'object' && options !== null &&
      Array.isArray(options.paths)) {
    const fakeParent = new Module('', null);

    paths = [];

    for (var i = 0; i < options.paths.length; i++) {
      const path = options.paths[i];
      fakeParent.paths = Module._nodeModulePaths(path);
      const lookupPaths = Module._resolveLookupPaths(request, fakeParent, true);

      if (!paths.includes(path))
        paths.push(path);

      for (var j = 0; j < lookupPaths.length; j++) {
        if (!paths.includes(lookupPaths[j]))
          paths.push(lookupPaths[j]);
      }
    }
  } else {
    paths = Module._resolveLookupPaths(request, parent, true);
  }

  // Look up the filename first, since that's the cache key.
  const filename = Module._findPath(request, paths, isMain);
  if (!filename) {
    // eslint-disable-next-line no-restricted-syntax
    var err = new Error(`Cannot find module '${request}'`);
    err.code = 'MODULE_NOT_FOUND';
    throw err;
  }
  return filename;
};


// Given a file name, pass it to the proper extension handler.
Module.prototype.load = function(filename) {
  debug('load %j for module %j', filename, this.id);

  assert(!this.loaded);
  this.filename = filename;
  this.paths = Module._nodeModulePaths(path.dirname(filename));

  const extension = findLongestRegisteredExtension(filename);
  Module._extensions[extension](this, filename);
  this.loaded = true;

  if (experimentalModules) {
    const ESMLoader = asyncESM.ESMLoader;
    const url = `${pathToFileURL(filename)}`;
    const module = ESMLoader.moduleMap.get(url);
    // Create module entry at load time to snapshot exports correctly
    const exports = this.exports;
    if (module !== undefined) { // called from cjs translator
      module.reflect.onReady((reflect) => {
        reflect.exports.default.set(exports);
      });
    } else { // preemptively cache
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
  requireDepth++;
  try {
    return Module._load(id, this, /* isMain */ false);
  } finally {
    requireDepth--;
  }
};


// Resolved path to process.argv[1] will be lazily placed here
// (needed for setting breakpoint when called with --inspect-brk)
var resolvedArgv;


// Run the file contents in the correct scope or sandbox. Expose
// the correct helper variables (require, module, exports) to
// the file.
// Returns exception, if any.
Module.prototype._compile = function(content, filename) {
  if (manifest) {
    const moduleURL = pathToFileURL(filename);
    manifest.assertIntegrity(moduleURL, content);
  }

  content = stripShebang(content);

  let compiledWrapper;
  if (patched) {
    const wrapper = Module.wrap(content);
    compiledWrapper = vm.runInThisContext(wrapper, {
      filename,
      lineOffset: 0,
      displayErrors: true,
      importModuleDynamically: experimentalModules ? async (specifier) => {
        const loader = await asyncESM.loaderPromise;
        return loader.import(specifier, normalizeReferrerURL(filename));
      } : undefined,
    });
  } else {
    compiledWrapper = compileFunction(
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
    if (experimentalModules) {
      const { callbackMap } = internalBinding('module_wrap');
      callbackMap.set(compiledWrapper, {
        importModuleDynamically: async (specifier) => {
          const loader = await asyncESM.loaderPromise;
          return loader.import(specifier, normalizeReferrerURL(filename));
        }
      });
    }
  }

  var inspectorWrapper = null;
  if (process._breakFirstLine && process._eval == null) {
    if (!resolvedArgv) {
      // We enter the repl if we're not given a filename argument.
      if (process.argv[1]) {
        resolvedArgv = Module._resolveFilename(process.argv[1], null, false);
      } else {
        resolvedArgv = 'repl';
      }
    }

    // Set breakpoint on module start
    if (filename === resolvedArgv) {
      delete process._breakFirstLine;
      inspectorWrapper = internalBinding('inspector').callAndPauseOnStart;
    }
  }
  const dirname = path.dirname(filename);
  const require = makeRequireFunction(this);
  var result;
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
  if (requireDepth === 0) statCache = null;
  return result;
};


// Native extension for .js
Module._extensions['.js'] = function(module, filename) {
  const content = fs.readFileSync(filename, 'utf8');
  module._compile(stripBOM(content), filename);
};


// Native extension for .json
Module._extensions['.json'] = function(module, filename) {
  const content = fs.readFileSync(filename, 'utf8');

  if (manifest) {
    const moduleURL = pathToFileURL(filename);
    manifest.assertIntegrity(moduleURL, content);
  }

  try {
    module.exports = JSON.parse(stripBOM(content));
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

if (experimentalModules) {
  Module._extensions['.mjs'] = function(module, filename) {
    throw new ERR_REQUIRE_ESM(filename);
  };
}

// bootstrap main module.
Module.runMain = function() {
  // Load the main module--the command line argument.
  if (experimentalModules) {
    asyncESM.loaderPromise.then((loader) => {
      return loader.import(pathToFileURL(process.argv[1]).pathname);
    })
    .catch((e) => {
      internalBinding('task_queue').triggerFatalException(e);
    });
  } else {
    Module._load(process.argv[1], null, true);
  }
  // Handle any nextTicks added in the first tick of the program
  process._tickCallback();
};

Module.createRequireFromPath = (filename) => {
  const m = new Module(filename);
  m.filename = filename;
  m.paths = Module._nodeModulePaths(path.dirname(filename));
  return makeRequireFunction(m);
};

Module._initPaths = function() {
  var homeDir;
  var nodePath;
  if (isWindows) {
    homeDir = process.env.USERPROFILE;
    nodePath = process.env.NODE_PATH;
  } else {
    homeDir = safeGetenv('HOME');
    nodePath = safeGetenv('NODE_PATH');
  }

  // $PREFIX/lib/node, where $PREFIX is the root of the Node.js installation.
  var prefixDir;
  // process.execPath is $PREFIX/bin/node except on Windows where it is
  // $PREFIX\node.exe.
  if (isWindows) {
    prefixDir = path.resolve(process.execPath, '..');
  } else {
    prefixDir = path.resolve(process.execPath, '..', '..');
  }
  var paths = [path.resolve(prefixDir, 'lib', 'node')];

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
  if (!Array.isArray(requests))
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
  for (var n = 0; n < requests.length; n++)
    parent.require(requests[n]);
};

Module._initPaths();

// Backwards compatibility
Module.Module = Module;

// We have to load the esm things after module.exports!
if (experimentalModules) {
  asyncESM = require('internal/process/esm_loader');
  ModuleJob = require('internal/modules/esm/module_job');
  createDynamicModule = require(
    'internal/modules/esm/create_dynamic_module');
}
