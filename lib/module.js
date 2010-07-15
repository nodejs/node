/**********************************************************************/

// Module


// Set the environ variable NODE_MODULE_CONTEXT=1 to make node load all
// modules in thier own context.
var contextLoad = false;
if (parseInt(process.env["NODE_MODULE_CONTEXTS"]) > 0) contextLoad = true;
var Script;

var internalModuleCache = {};
var extensionCache = {};

function Module (id, parent) {
  this.id = id;
  this.exports = {};
  this.parent = parent;

  if (parent) {
    this.moduleCache = parent.moduleCache;
  } else {
    this.moduleCache = {};
  }
  this.moduleCache[this.id] = this;

  this.filename = null;
  this.loaded = false;
  this.exited = false;
  this.children = [];
};

function createInternalModule (id, constructor) {
  var m = new Module(id);
  constructor(m.exports);
  m.loaded = true;
  internalModuleCache[id] = m;
  return m;
};


// This contains the source code for the files in lib/
// Like, natives.fs is the contents of lib/fs.js
var natives = process.binding('natives');

function loadNative (id) {
  var m = new Module(id);
  internalModuleCache[id] = m;
  var e = m._compile(natives[id], id);
  if (e) throw e;
  m.loaded = true;
  return m;
}

exports.requireNative = requireNative;

function requireNative (id) {
  if (internalModuleCache[id]) return internalModuleCache[id].exports;
  if (!natives[id]) throw new Error('No such native module ' + id);
  return loadNative(id).exports;
}



// Event

var eventsFn = process.compile("(function (exports) {" + natives.events + "\n})",
                               "events");
var eventsModule = createInternalModule('events', eventsFn);
var events = eventsModule.exports;


// Modules

var debugLevel = parseInt(process.env["NODE_DEBUG"], 16);
function debug (x) {
  if (debugLevel & 1) {
    process.binding('stdio').writeError(x + "\n");
  }
}

var pathFn = process.compile("(function (exports) {" + natives.path + "\n})",
                             "path");
var pathModule = createInternalModule('path', pathFn);
var path = pathModule.exports;

function existsSync (path) {
  try {
    process.binding('fs').stat(path);
    return true;
  } catch (e) {
    return false;
  }
}



var modulePaths = [path.join(process.execPath, "..", "..", "lib", "node")];

if (process.env["HOME"]) {
  modulePaths.unshift(path.join(process.env["HOME"], ".node_libraries"));
}

if (process.env["NODE_PATH"]) {
  modulePaths = process.env["NODE_PATH"].split(":").concat(modulePaths);
}


/* Sync unless callback given */
function findModulePath (id, dirs, callback) {
  process.assert(dirs.constructor == Array);

  if (/^https?:\/\//.exec(id)) {
    if (callback) {
      callback(id);
    } else {
      throw new Error("Sync http require not allowed.");
    }
    return;
  }

  if (/\.(js|node)$/.exec(id)) {
    throw new Error("No longer accepting filename extension in module names");
  }

  if (dirs.length == 0) {
    if (callback) {
      callback();
    } else {
      return; // sync returns null
    }
  }

  var dir = dirs[0];
  var rest = dirs.slice(1, dirs.length);

  if (id.charAt(0) == '/') {
    dir = '';
    rest = [];
  }

  var locations = [
    path.join(dir, id + ".js"),
    path.join(dir, id + ".node"),
    path.join(dir, id, "index.js"),
    path.join(dir, id, "index.node")
  ];

  var ext;
  var extensions = Object.keys(extensionCache);
  for (var i = 0, l = extensions.length; i < l; i++) {
    var ext = extensions[i];
    locations.push(path.join(dir, id + ext));
    locations.push(path.join(dir, id, 'index' + ext));
  }

  function searchLocations () {
    var location = locations.shift();
    if (!location) {
      return findModulePath(id, rest, callback);
    }

    // if async
    if (callback) {
      path.exists(location, function (found) {
        if (found) {
          callback(location);
        } else {
          return searchLocations();
        }
      });

    // if sync
    } else {
      if (existsSync(location)) {
        return location;
      } else {
        return searchLocations();
      }
    }
  }
  return searchLocations();
}


// sync - no i/o performed
function resolveModulePath(request, parent) {
  var id, paths;
  if (request.charAt(0) == "." && (request.charAt(1) == "/" || request.charAt(1) == ".")) {
    // Relative request

    var exts = ['js', 'node'], ext;
    var extensions = Object.keys(extensionCache);
    for (var i = 0, l = extensions.length; i < l; i++) {
      var ext = extensions[i];
      exts.push(ext.slice(1));
    }

    var parentIdPath = path.dirname(parent.id +
      (path.basename(parent.filename).match(new RegExp('^index\\.(' + exts.join('|') + ')$')) ? "/" : ""));
    id = path.join(parentIdPath, request);
    // make sure require('./path') and require('path') get distinct ids, even
    // when called from the toplevel js file
    if (parentIdPath == '.' && ! id.match(new RegExp('/'))) {
      id = './' + id;
    }
    debug("RELATIVE: requested:" + request + " set ID to: "+id+" from "+parent.id);
    paths = [path.dirname(parent.filename)];
  } else {
    id = request;
    // debug("ABSOLUTE: id="+id);
    paths = modulePaths;
  }

  return [id, paths];
}


function loadModule (request, parent, callback) {
  var resolvedModule = resolveModulePath(request, parent),
      id = resolvedModule[0],
      paths = resolvedModule[1];

  debug("loadModule REQUEST  " + (request) + " parent: " + parent.id);

  var cachedModule = internalModuleCache[id] || parent.moduleCache[id];

  if (!cachedModule) {
    // Try to compile from native modules
    if (natives[id]) {
      debug('load native module ' + id);
      cachedModule = loadNative(id);
    }
  }

  if (cachedModule) {
    debug("found  " + JSON.stringify(id) + " in cache");
    if (callback) {
      callback(null, cachedModule.exports);
    } else {
      return cachedModule.exports;
    }

  } else {
    // Not in cache
    debug("looking for " + JSON.stringify(id) + " in " + JSON.stringify(paths));

    if (!callback) {
      // sync
      var filename = findModulePath(request, paths);
      if (!filename) {
        throw new Error("Cannot find module '" + request + "'");
      } else {
        var module = new Module(id, parent);
        module.loadSync(filename);
        return module.exports;
      }

    } else {
      // async
      findModulePath(request, paths, function (filename) {
        if (!filename) {
          var err = new Error("Cannot find module '" + request + "'");
          callback(err);
        } else {
          var module = new Module(id, parent);
          module.load(filename, callback);
        }
      });
    }
  }
};


// This function allows the user to register file extensions to custom
// Javascript 'compilers'.  It accepts 2 arguments, where ext is a file
// extension as a string. E.g. '.coffee' for coffee-script files.  compiler
// is the second argument, which is a function that gets called when the
// specified file extension is found. The compiler is passed a single
// argument, which is, the file contents, which need to be compiled.
//
// The function needs to return the compiled source, or an non-string
// variable that will get attached directly to the module exports. Example:
//
//    require.registerExtension('.coffee', function(content) {
//      return doCompileMagic(content);
//    });
function registerExtension(ext, compiler) {
  if ('string' !== typeof ext && false === /\.\w+$/.test(ext)) {
    throw new Error('require.registerExtension: First argument not a valid extension string.');
  }

  if ('function' !== typeof compiler) {
    throw new Error('require.registerExtension: Second argument not a valid compiler function.');
  }

  extensionCache[ext] = compiler;
}


Module.prototype.loadSync = function (filename) {
  debug("loadSync " + JSON.stringify(filename) + " for module " + JSON.stringify(this.id));

  process.assert(!this.loaded);
  this.filename = filename;

  if (filename.match(/\.node$/)) {
    this._loadObjectSync(filename);
  } else {
    this._loadScriptSync(filename);
  }
};


Module.prototype.load = function (filename, callback) {
  debug("load " + JSON.stringify(filename) + " for module " + JSON.stringify(this.id));

  process.assert(!this.loaded);

  this.filename = filename;

  if (filename.match(/\.node$/)) {
    this._loadObject(filename, callback);
  } else {
    this._loadScript(filename, callback);
  }
};


Module.prototype._loadObjectSync = function (filename) {
  this.loaded = true;
  process.dlopen(filename, this.exports);
};


Module.prototype._loadObject = function (filename, callback) {
  var self = this;
  // XXX Not yet supporting loading from HTTP. would need to download the
  // file, store it to tmp then run dlopen on it.
  self.loaded = true;
  process.dlopen(filename, self.exports); // FIXME synchronus
  if (callback) callback(null, self.exports);
};


function cat (id, callback) {
  if (id.match(/^http:\/\//)) {
    loadModule('http', process.mainModule, function (err, http) {
      if (err) {
        if (callback) callback(err);
      } else {
        http.cat(id, callback);
      }
    });
  } else {
    requireNative('fs').readFile(id, 'utf8', callback);
  }
}


// Returns exception if any
Module.prototype._compile = function (content, filename) {
  var self = this;
  // remove shebang
  content = content.replace(/^\#\!.*/, '');

  // Compile content if needed
  var ext = path.extname(filename);
  if (extensionCache[ext]) {
    content = extensionCache[ext](content);
  }

  function requireAsync (url, cb) {
    loadModule(url, self, cb);
  }

  function require (path) {
    return loadModule(path, self);
  }

  require.paths = modulePaths;
  require.async = requireAsync;
  require.main = process.mainModule;
  require.registerExtension = registerExtension;

  var dirname = path.dirname(filename);

  if (contextLoad) {
    if (!Script) Script = Script = process.binding('evals').Script;

    if (self.id !== ".") {
      debug('load submodule');
      // not root module
      var sandbox = {};
      for (var k in global) {
        sandbox[k] = global[k];
      }
      sandbox.require     = require;
      sandbox.exports     = self.exports;
      sandbox.__filename  = filename;
      sandbox.__dirname   = dirname;
      sandbox.module      = self;
      sandbox.root        = global;

      Script.runInNewContext(content, sandbox, filename);

    } else {
      debug('load root module');
      // root module
      global.require    = require;
      global.exports    = self.exports;
      global.__filename = filename;
      global.__dirname  = dirname;
      global.module     = self;
      global.root       = global;
      Script.runInThisContext(content, filename);
    }

  } else {
    if ('string' === typeof content) {
      // create wrapper function
      var wrapper = "(function (exports, require, module, __filename, __dirname) { "
                  + content
                  + "\n});";

      var compiledWrapper = process.compile(wrapper, filename);
      compiledWrapper.apply(self.exports, [self.exports, require, self, filename, dirname]);
    } else {
      self.exports = content;
    }
  }
};


Module.prototype._loadScriptSync = function (filename) {
  var content = requireNative('fs').readFileSync(filename, 'utf8');
  this._compile(content, filename);
  this.loaded = true;
};


Module.prototype._loadScript = function (filename, callback) {
  var self = this;
  cat(filename, function (err, content) {
    debug('cat done');
    if (err) {
      if (callback) callback(err);
    } else {
      try {
        self._compile(content, filename);
      } catch (err) {
        if (callback) callback(err);
        return;
      }

      self._waitChildrenLoad(function () {
        self.loaded = true;
        if (self.onload) self.onload();
        if (callback) callback(null, self.exports);
      });
    }
  });
};


Module.prototype._waitChildrenLoad = function (callback) {
  var nloaded = 0;
  var children = this.children;
  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    if (child.loaded) {
      nloaded++;
    } else {
      child.onload = function () {
        child.onload = null;
        nloaded++;
        if (children.length == nloaded && callback) callback();
      };
    }
  }
  if (children.length == nloaded && callback) callback();
};



// bootstrap main module.
exports.runMain = function () {

  // Load the main module--the command line argument.
  process.mainModule = new Module(".");
  process.mainModule.loadSync(process.argv[1]);
}
