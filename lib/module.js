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

var NativeModule = require('native_module');
var Script = process.binding('evals').Script;
var runInThisContext = Script.runInThisContext;
var runInNewContext = Script.runInNewContext;
var assert = require('assert').ok;

function Module(id, parent) {
  this.id = id;
  this.exports = {};
  this.parent = parent;

  this.filename = null;
  this.loaded = false;
  this.exited = false;
  this.children = [];
}
module.exports = Module;

// Set the environ variable NODE_MODULE_CONTEXTS=1 to make node load all
// modules in thier own context.
Module._contextLoad = (+process.env['NODE_MODULE_CONTEXTS'] > 0);
Module._cache = {};
Module._pathCache = {};
Module._extensions = {};
Module._paths = [];

Module.wrapper = NativeModule.wrapper;
Module.wrap = NativeModule.wrap;

var path = NativeModule.require('path');

Module._debug = function() {};
if (process.env.NODE_DEBUG && /module/.test(process.env.NODE_DEBUG)) {
  Module._debug = function(x) {
    console.error(x);
  };
}


// We use this alias for the preprocessor that filters it out
var debug = Module._debug;


// given a module name, and a list of paths to test, returns the first
// matching file in the following precedence.
//
// require("a.<ext>")
//   -> a.<ext>
//
// require("a")
//   -> a
//   -> a.<ext>
//   -> a/index.<ext>

function statPath(path) {
  var fs = NativeModule.require('fs');
  try {
    return fs.statSync(path);
  } catch (ex) {}
  return false;
}

// check if the directory is a package.json dir
var packageCache = {};

function readPackage(requestPath) {
  if (packageCache.hasOwnProperty(requestPath)) {
    return packageCache[requestPath];
  }

  var fs = NativeModule.require('fs');
  try {
    var jsonPath = path.resolve(requestPath, 'package.json');
    var json = fs.readFileSync(jsonPath, 'utf8');
    var pkg = packageCache[requestPath] = JSON.parse(json);
    return pkg;
  } catch (e) {}

  return false;
}

function tryPackage(requestPath, exts) {
  var pkg = readPackage(requestPath);

  if (!pkg || !pkg.main) return false;

  var filename = path.resolve(requestPath, pkg.main);
  return tryFile(filename) || tryExtensions(filename, exts) ||
         tryExtensions(path.resolve(filename, 'index'), exts);
}

// In order to minimize unnecessary lstat() calls,
// this cache is a list of known-real paths.
// Set to an empty object to reset.
Module._realpathCache = {}

// check if the file exists and is not a directory
function tryFile(requestPath) {
  var fs = NativeModule.require('fs');
  var stats = statPath(requestPath);
  if (stats && !stats.isDirectory()) {
    return fs.realpathSync(requestPath, Module._realpathCache);
  }
  return false;
}

// given a path check a the file exists with any of the set extensions
function tryExtensions(p, exts) {
  for (var i = 0, EL = exts.length; i < EL; i++) {
    var filename = tryFile(p + exts[i]);

    if (filename) {
      return filename;
    }
  }
  return false;
}


Module._findPath = function(request, paths) {
  var fs = NativeModule.require('fs');
  var exts = Object.keys(Module._extensions);

  if (request.charAt(0) === '/') {
    paths = [''];
  }

  var trailingSlash = (request.slice(-1) === '/');

  var cacheKey = JSON.stringify({request: request, paths: paths});
  if (Module._pathCache[cacheKey]) {
    return Module._pathCache[cacheKey];
  }

  // For each path
  for (var i = 0, PL = paths.length; i < PL; i++) {
    var basePath = path.resolve(paths[i], request);
    var filename;

    if (!trailingSlash) {
      // try to join the request to the path
      filename = tryFile(basePath);

      if (!filename && !trailingSlash) {
        // try it with each of the extensions
        filename = tryExtensions(basePath, exts);
      }
    }

    if (!filename) {
      filename = tryPackage(basePath, exts);
    }

    if (!filename) {
      // try it with each of the extensions at "index"
      filename = tryExtensions(path.resolve(basePath, 'index'), exts);
    }

    if (filename) {
      Module._pathCache[cacheKey] = filename;
      return filename;
    }
  }
  return false;
};

// 'from' is the __dirname of the module.
Module._nodeModulePaths = function(from) {
  // guarantee that 'from' is absolute.
  from = path.resolve(from);

  // note: this approach *only* works when the path is guaranteed
  // to be absolute.  Doing a fully-edge-case-correct path.split
  // that works on both Windows and Posix is non-trivial.
  var splitRe = process.platform === 'win32' ? /[\/\\]/ : /\//;
  // yes, '/' works on both, but let's be a little canonical.
  var joiner = process.platform === 'win32' ? '\\' : '/';
  var paths = [];
  var parts = from.split(splitRe);

  for (var tip = parts.length - 1; tip >= 0; tip --) {
    // don't search in .../node_modules/node_modules
    if (parts[tip] === 'node_modules') continue;
    var dir = parts.slice(0, tip + 1).concat('node_modules').join(joiner);
    paths.push(dir);
  }

  return paths;
}


Module._resolveLookupPaths = function(request, parent) {
  if (NativeModule.exists(request)) {
    return [request, []];
  }

  var start = request.substring(0, 2);
  if (start !== './' && start !== '..') {
    var paths = Module._paths;
    if (parent) {
      if (!parent.paths) parent.paths = [];
      paths = parent.paths.concat(paths);
    }
    return [request, paths];
  }

  // with --eval, parent.id is not set and parent.filename is null
  if (!parent || !parent.id || !parent.filename) {
    // make require('./path/to/foo') work - normally the path is taken
    // from realpath(__filename) but with eval there is no filename
    var mainPaths = ['.'].concat(Module._paths);
    mainPaths = Module._nodeModulePaths('.').concat(mainPaths);
    return [request, mainPaths];
  }

  // Is the parent an index module?
  // We can assume the parent has a valid extension,
  // as it already has been accepted as a module.
  var isIndex = /^index\.\w+?$/.test(path.basename(parent.filename));
  var parentIdPath = isIndex ? parent.id : path.dirname(parent.id);
  var id = path.resolve(parentIdPath, request);

  // make sure require('./path') and require('path') get distinct ids, even
  // when called from the toplevel js file
  if (parentIdPath === '.' && id.indexOf('/') === -1) {
    id = './' + id;
  }

  debug('RELATIVE: requested:' + request +
        ' set ID to: ' + id + ' from ' + parent.id);

  return [id, [path.dirname(parent.filename)]];
};


Module._load = function(request, parent, isMain) {
  if (parent) {
    debug('Module._load REQUEST  ' + (request) + ' parent: ' + parent.id);
  }

  var resolved = Module._resolveFilename(request, parent);
  var id = resolved[0];
  var filename = resolved[1];

  var cachedModule = Module._cache[filename];
  if (cachedModule) {
    return cachedModule.exports;
  }

  if (NativeModule.exists(id)) {
    // REPL is a special case, because it needs the real require.
    if (id == 'repl') {
      var replModule = new Module('repl');
      replModule._compile(NativeModule.getSource('repl'), 'repl.js');
      NativeModule._cache.repl = replModule;
      return replModule.exports;
    }

    debug('load native module ' + request);
    return NativeModule.require(id);
  }

  var module = new Module(id, parent);

  if (isMain) {
    process.mainModule = module;
    module.id = '.';
  }

  Module._cache[filename] = module;
  try {
    module.load(filename);
  } catch (err) {
    delete Module._cache[filename];
    throw err;
  }

  return module.exports;
};

Module._resolveFilename = function(request, parent) {
  if (NativeModule.exists(request)) {
    return [request, request];
  }

  var resolvedModule = Module._resolveLookupPaths(request, parent);
  var id = resolvedModule[0];
  var paths = resolvedModule[1];

  // look up the filename first, since that's the cache key.
  debug('looking for ' + JSON.stringify(id) +
        ' in ' + JSON.stringify(paths));

  var filename = Module._findPath(request, paths);
  if (!filename) {
    throw new Error("Cannot find module '" + request + "'");
  }
  id = filename;
  return [id, filename];
};


Module.prototype.load = function(filename) {
  debug('load ' + JSON.stringify(filename) +
        ' for module ' + JSON.stringify(this.id));

  assert(!this.loaded);
  this.filename = filename;
  this.paths = Module._nodeModulePaths(path.dirname(filename));

  var extension = path.extname(filename) || '.js';
  if (!Module._extensions[extension]) extension = '.js';
  Module._extensions[extension](this, filename);
  this.loaded = true;
};


// Returns exception if any
Module.prototype._compile = function(content, filename) {
  var self = this;
  // remove shebang
  content = content.replace(/^\#\!.*/, '');

  function require(path) {
    return Module._load(path, self);
  }

  require.resolve = function(request) {
    return Module._resolveFilename(request, self)[1];
  }
  require.paths = Module._paths;
  require.main = process.mainModule;
  // Enable support to add extra extension types
  require.extensions = Module._extensions;
  require.registerExtension = function() {
    throw new Error('require.registerExtension() removed. Use ' +
                    'require.extensions instead.');
  }
  require.cache = Module._cache;

  var dirname = path.dirname(filename);

  if (Module._contextLoad) {
    if (self.id !== '.') {
      debug('load submodule');
      // not root module
      var sandbox = {};
      for (var k in global) {
        sandbox[k] = global[k];
      }
      sandbox.require = require;
      sandbox.exports = self.exports;
      sandbox.__filename = filename;
      sandbox.__dirname = dirname;
      sandbox.module = self;
      sandbox.global = sandbox;
      sandbox.root = root;

      return runInNewContext(content, sandbox, filename, true);
    }

    debug('load root module');
    // root module
    global.require = require;
    global.exports = self.exports;
    global.__filename = filename;
    global.__dirname = dirname;
    global.module = self;

    return runInThisContext(content, filename, true);
  }

  // create wrapper function
  var wrapper = Module.wrap(content);

  var compiledWrapper = runInThisContext(wrapper, filename, true);
  if (filename === process.argv[1] && global.v8debug) {
    global.v8debug.Debug.setBreakPoint(compiledWrapper, 0, 0);
  }
  var args = [self.exports, require, self, filename, dirname];
  return compiledWrapper.apply(self.exports, args);
};

// Native extension for .js
Module._extensions['.js'] = function(module, filename) {
  var content = NativeModule.require('fs').readFileSync(filename, 'utf8');
  module._compile(content, filename);
};


// Native extension for .node
Module._extensions['.node'] = function(module, filename) {
  process.dlopen(filename, module.exports);
};


// bootstrap main module.
Module.runMain = function() {
  // Load the main module--the command line argument.
  Module._load(process.argv[1], null, true);
};

Module._initPaths = function() {
  var paths = [path.resolve(process.execPath, '..', '..', 'lib', 'node')];

  if (process.env['HOME']) {
    paths.unshift(path.resolve(process.env['HOME'], '.node_libraries'));
    paths.unshift(path.resolve(process.env['HOME'], '.node_modules'));
  }

  if (process.env['NODE_PATH']) {
    paths = process.env['NODE_PATH'].split(':').concat(paths);
  }

  Module._paths = paths;
};

// bootstrap repl
Module.requireRepl = function() {
  return Module._load('repl', '.');
};

Module._initPaths();

// backwards compatibility
Module.Module = Module;
