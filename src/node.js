(function (process) {

process.global.process = process;
process.global.global = process.global;
global.GLOBAL = global;

/** deprecation errors ************************************************/

function removed (reason) {
  return function () {
    throw new Error(reason)
  }
}

GLOBAL.__module = removed("'__module' has been renamed to 'module'");
GLOBAL.include = removed("include(module) has been removed. Use require(module)");
GLOBAL.puts = removed("puts() has moved. Use require('sys') to bring it back.");
GLOBAL.print = removed("print() has moved. Use require('sys') to bring it back.");
GLOBAL.p = removed("p() has moved. Use require('sys') to bring it back.");
process.debug = removed("process.debug() has moved. Use require('sys') to bring it back.");
process.error = removed("process.error() has moved. Use require('sys') to bring it back.");
process.watchFile = removed("process.watchFile() has moved to fs.watchFile()");
process.unwatchFile = removed("process.unwatchFile() has moved to fs.unwatchFile()");

GLOBAL.node = {};

node.createProcess = removed("node.createProcess() has been changed to process.createChildProcess() update your code");
node.exec = removed("process.exec() has moved. Use require('sys') to bring it back.");
node.inherits = removed("node.inherits() has moved. Use require('sys') to access it.");
process.inherits = removed("process.inherits() has moved to sys.inherits.");

node.http = {};
node.http.createServer = removed("node.http.createServer() has moved. Use require('http') to access it.");
node.http.createClient = removed("node.http.createClient() has moved. Use require('http') to access it.");

node.tcp = {};
node.tcp.createServer = removed("node.tcp.createServer() has moved. Use require('tcp') to access it.");
node.tcp.createConnection = removed("node.tcp.createConnection() has moved. Use require('tcp') to access it.");

node.dns = {};
node.dns.createConnection = removed("node.dns.createConnection() has moved. Use require('dns') to access it.");

/**********************************************************************/

// Module 

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


process.createChildProcess = function (file, args, env) {
  var child = new process.ChildProcess();
  args = args || [];
  env = env || process.env;
  var envPairs = [];
  for (var key in env) {
    if (env.hasOwnProperty(key)) {
      envPairs.push(key + "=" + env[key]);
    }
  }
  // TODO Note envPairs is not currently used in child_process.cc. The PATH
  // needs to be searched for the 'file' command if 'file' does not contain
  // a '/' character.
  child.spawn(file, args, envPairs);
  return child;
};

process.assert = function (x, msg) {
  if (!(x)) throw new Error(msg || "assertion error");
};

// From jQuery.extend in the jQuery JavaScript Library v1.3.2
// Copyright (c) 2009 John Resig
// Dual licensed under the MIT and GPL licenses.
// http://docs.jquery.com/License
// Modified for node.js (formely for copying properties correctly)
var mixinMessage;
process.mixin = function() {
  if (!mixinMessage) {
    mixinMessage = 'deprecation warning: process.mixin will be removed from node-core future releases.\n'
    process.stdio.writeError(mixinMessage);
  }
  // copy reference to target object
  var target = arguments[0] || {}, i = 1, length = arguments.length, deep = false, source;

  // Handle a deep copy situation
  if ( typeof target === "boolean" ) {
    deep = target;
    target = arguments[1] || {};
    // skip the boolean and the target
    i = 2;
  }

  // Handle case when target is a string or something (possible in deep copy)
  if ( typeof target !== "object" && !(typeof target === 'function') )
    target = {};

  // mixin process itself if only one argument is passed
  if ( length == i ) {
    target = GLOBAL;
    --i;
  }

  for ( ; i < length; i++ ) {
    // Only deal with non-null/undefined values
    if ( (source = arguments[i]) != null ) {
      // Extend the base object
      Object.getOwnPropertyNames(source).forEach(function(k){
        var d = Object.getOwnPropertyDescriptor(source, k) || {value: source[k]};
        if (d.get) {
          target.__defineGetter__(k, d.get);
          if (d.set) {
            target.__defineSetter__(k, d.set);
          }
        }
        else {
          // Prevent never-ending loop
          if (target === d.value) {
            continue;
          }

          if (deep && d.value && typeof d.value === "object") {
            target[k] = process.mixin(deep,
              // Never move original objects, clone them
              source[k] || (d.value.length != null ? [] : {})
            , d.value);
          }
          else {
            target[k] = d.value;
          }
        }
      });
    }
  }
  // Return the modified object
  return target;
};

// Event

var eventsModule = createInternalModule('events', function (exports) {
  exports.EventEmitter = process.EventEmitter;

  // process.EventEmitter is defined in src/events.cc
  // process.EventEmitter.prototype.emit() is also defined there.
  process.EventEmitter.prototype.addListener = function (type, listener) {
    if (!(listener instanceof Function)) {
      throw new Error('addListener only takes instances of Function');
    }

    if (!this._events) this._events = {};

    // To avoid recursion in the case that type == "newListeners"! Before
    // adding it to the listeners, first emit "newListeners".
    this.emit("newListener", type, listener);

    if (!this._events[type]) {
      // Optimize the case of one listener. Don't need the extra array object.
      this._events[type] = listener;
    } else if (this._events[type] instanceof Array) {
      // If we've already got an array, just append.
      this._events[type].push(listener);
    } else {
      // Adding the second element, need to change to array.
      this._events[type] = [this._events[type], listener];
    }

    return this;
  };

  process.EventEmitter.prototype.removeListener = function (type, listener) {
    if (!(listener instanceof Function)) {
      throw new Error('removeListener only takes instances of Function');
    }

    // does not use listeners(), so no side effect of creating _events[type]
    if (!this._events || !this._events[type]) return this;

    var list = this._events[type];

    if (list instanceof Array) {
      var i = list.indexOf(listener);
      if (i < 0) return this;
      list.splice(i, 1);
    } else {
      this._events[type] = null;
    }

    return this;
  };

  process.EventEmitter.prototype.removeAllListeners = function (type) {
    // does not use listeners(), so no side effect of creating _events[type]
    if (!type || !this._events || !this._events[type]) return this;
    this._events[type] = null;
  };

  process.EventEmitter.prototype.listeners = function (type) {
    if (!this._events) this._events = {};
    if (!this._events[type]) this._events[type] = [];
    if (!(this._events[type] instanceof Array)) {
      this._events[type] = [this._events[type]];
    }
    return this._events[type];
  };

  exports.Promise = removed('Promise has been removed. See http://groups.google.com/group/nodejs/msg/0c483b891c56fea2 for more information.');
  process.Promise = exports.Promise;
});

var events = eventsModule.exports;


// nextTick()

var nextTickQueue = [];
var nextTickWatcher = new process.IdleWatcher();
// Only debugger has maximum priority. Below that is the nextTickWatcher.
nextTickWatcher.setPriority(process.EVMAXPRI-1);

nextTickWatcher.callback = function () {
  var l = nextTickQueue.length;
  while (l--) {
    var cb = nextTickQueue.shift();
    cb();
  }
  if (nextTickQueue.length == 0) nextTickWatcher.stop();
};

process.nextTick = function (callback) {
  nextTickQueue.push(callback);
  nextTickWatcher.start();
};





// Signal Handlers

function isSignal (event) {
  return event.slice(0, 3) === 'SIG' && process.hasOwnProperty(event);
};

process.addListener("newListener", function (event) {
  if (isSignal(event) && process.listeners(event).length === 0) {
    var handler = new process.SignalHandler(process[event]);
    handler.addListener("signal", function () {
      process.emit(event);
    });
  }
});


// Timers
function addTimerListener (callback) {
  var timer = this;
  // Special case the no param case to avoid the extra object creation.
  if (arguments.length > 2) {
    var args = Array.prototype.slice.call(arguments, 2);
    timer.callback = function () { callback.apply(timer, args); };
  } else {
    timer.callback = callback;
  }
}

global.setTimeout = function (callback, after) {
  var timer = new process.Timer();
  addTimerListener.apply(timer, arguments);
  timer.start(after, 0);
  return timer;
};

global.setInterval = function (callback, repeat) {
  var timer = new process.Timer();
  addTimerListener.apply(timer, arguments);
  timer.start(repeat, repeat);
  return timer;
};

global.clearTimeout = function (timer) {
  if (timer instanceof process.Timer) {
    timer.stop();
  }
};

global.clearInterval = global.clearTimeout;




// Modules

var debugLevel = 0;
if ("NODE_DEBUG" in process.env) debugLevel = 1;

function debug (x) {
  if (debugLevel > 0) {
    process.stdio.writeError(x + "\n");
  }
}




function readAll (fd, pos, content, encoding, callback) {
  process.fs.read(fd, 4*1024, pos, encoding, function (err, chunk, bytesRead) {
    if (err) {
      if (callback) callback(err);
    } else if (chunk) {
      content += chunk;
      pos += bytesRead;
      readAll(fd, pos, content, encoding, callback);
    } else {
      process.fs.close(fd, function (err) {
        if (callback) callback(err, content);
      });
    }
  });
}

process.fs.readFile = function (path, encoding_, callback) {
  var encoding = typeof(encoding_) == 'string' ? encoding_ : 'utf8';
  var callback_ = arguments[arguments.length - 1];
  var callback = (typeof(callback_) == 'function' ? callback_ : null);
  process.fs.open(path, process.O_RDONLY, 0666, function (err, fd) {
    if (err) {
      if (callback) callback(err); 
    } else {
      readAll(fd, 0, "", encoding, callback);
    }
  });
};

process.fs.readFileSync = function (path, encoding) {
  encoding = encoding || "utf8"; // default to utf8

  debug('readFileSync open');

  var fd = process.fs.open(path, process.O_RDONLY, 0666);
  var content = '';
  var pos = 0;
  var r;

  while ((r = process.fs.read(fd, 4*1024, pos, encoding)) && r[0]) {
    debug('readFileSync read ' + r[1]);
    content += r[0];
    pos += r[1]
  }

  debug('readFileSync close');

  process.fs.close(fd);

  debug('readFileSync done');

  return content;
};

var pathModule = createInternalModule("path", function (exports) {
  exports.join = function () {
    return exports.normalize(Array.prototype.join.call(arguments, "/"));
  };

  exports.normalizeArray = function (parts, keepBlanks) {
    var directories = [], prev;
    for (var i = 0, l = parts.length - 1; i <= l; i++) {
      var directory = parts[i];

      // if it's blank, but it's not the first thing, and not the last thing, skip it.
      if (directory === "" && i !== 0 && i !== l && !keepBlanks) continue;

      // if it's a dot, and there was some previous dir already, then skip it.
      if (directory === "." && prev !== undefined) continue;

      if (
        directory === ".."
        && directories.length
        && prev !== ".."
        && prev !== undefined
        && (prev !== "" || keepBlanks)
      ) {
        directories.pop();
        prev = directories.slice(-1)[0]
      } else {
        if (prev === ".") directories.pop();
        directories.push(directory);
        prev = directory;
      }
    }
    return directories;
  };

  exports.normalize = function (path, keepBlanks) {
    return exports.normalizeArray(path.split("/"), keepBlanks).join("/");
  };

  exports.dirname = function (path) {
    return path.substr(0, path.lastIndexOf("/")) || ".";
  };

  exports.filename = function () {
    throw new Error("path.filename is deprecated. Please use path.basename instead.");
  };
  exports.basename = function (path, ext) {
    var f = path.substr(path.lastIndexOf("/") + 1);
    if (ext && f.substr(-1 * ext.length) === ext) {
      f = f.substr(0, f.length - ext.length);
    }
    return f;
  };

  exports.extname = function (path) {
    var index = path.lastIndexOf('.');
    return index < 0 ? '' : path.substring(index);
  };

  exports.exists = function (path, callback) {
    process.fs.stat(path, function (err, stats) {
      if (callback) callback(err ? false : true);
    });
  };
});

var path = pathModule.exports;

function existsSync (path) {
  try {
    process.fs.stat(path);
    return true;
  } catch (e) {
    return false;
  }
}



process.paths = [ path.join(process.installPrefix, "lib/node/libraries")
               ];

if (process.env["HOME"]) {
  process.paths.unshift(path.join(process.env["HOME"], ".node_libraries"));
}

if (process.env["NODE_PATH"]) {
  process.paths = process.env["NODE_PATH"].split(":").concat(process.paths);
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
  for (ext in extensionCache) {
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
    for (ext in extensionCache) {
      exts.push(ext.slice(1));
    }

    var parentIdPath = path.dirname(parent.id +
      (path.basename(parent.filename).match(new RegExp('^index\\.(' + exts.join('|') + ')$')) ? "/" : ""));
    id = path.join(parentIdPath, request);
    // debug("RELATIVE: requested:"+request+" set ID to: "+id+" from "+parent.id+"("+parentIdPath+")");
    paths = [path.dirname(parent.filename)];
  } else {
    id = request;
    // debug("ABSOLUTE: id="+id);
    paths = process.paths;
  }

  return [id, paths];
}


function loadModuleSync (request, parent) {
  var resolvedModule = resolveModulePath(request, parent);
  var id = resolvedModule[0];
  var paths = resolvedModule[1];

  debug("loadModuleSync REQUEST  " + (request) + " parent: " + parent.id);

  var cachedModule = internalModuleCache[id] || parent.moduleCache[id];

  if (cachedModule) {
    debug("found  " + JSON.stringify(id) + " in cache");
    return cachedModule.exports;
  } else {
    debug("looking for " + JSON.stringify(id) + " in " + JSON.stringify(paths));
    var filename = findModulePath(request, paths);
    if (!filename) {
      throw new Error("Cannot find module '" + request + "'");
    } else {
      var module = new Module(id, parent);
      module.loadSync(filename);
      return module.exports;
    }
  }
}


function loadModule (request, parent, callback) {
  var
    resolvedModule = resolveModulePath(request, parent),
    id = resolvedModule[0],
    paths = resolvedModule[1];

  debug("loadModule REQUEST  " + (request) + " parent: " + parent.id);

  var cachedModule = internalModuleCache[id] || parent.moduleCache[id];
  if (cachedModule) {
    debug("found  " + JSON.stringify(id) + " in cache");
    if (callback) callback(null, cachedModule.exports);
   } else {
    debug("looking for " + JSON.stringify(id) + " in " + JSON.stringify(paths));
    // Not in cache
    findModulePath(request, paths, function (filename) {
      if (!filename) {
        var err = new Error("Cannot find module '" + request + "'");
        if (callback) callback(err);
      } else {
        var module = new Module(id, parent);
        module.load(filename, callback);
      }
    });
  }
};


// This function allows the user to register file extensions to custom
// Javascript 'compilers'.  It accepts 2 arguments, where ext is a file
// extension as a string. E.g. '.coffee' for coffee-script files.  compiler
// is the second argument, which is a function that gets called then the
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
    process.fs.readFile(id, callback);
  }
}


Module.prototype._loadContent = function (content, filename) {
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
    return loadModuleSync(path, self);
  }

  require.paths = process.paths;
  require.async = requireAsync;
  require.main = process.mainModule;
  require.registerExtension = registerExtension;


  if ('string' === typeof content) {
    // create wrapper function
    var wrapper = "(function (exports, require, module, __filename, __dirname) { "
                + content
                + "\n});";

    try {
      var compiledWrapper = process.compile(wrapper, filename);
      var dirName = path.dirname(filename);
      if (filename === process.argv[1]) {
        process.checkBreak();
      }
      compiledWrapper.apply(self.exports, [self.exports, require, self, filename, dirName]);
    } catch (e) {
      return e;
    }
  } else {
    self.exports = content;
  }
};


Module.prototype._loadScriptSync = function (filename) {
  var content = process.fs.readFileSync(filename);
  // remove shebang
  content = content.replace(/^\#\!.*/, '');

  var e = this._loadContent(content, filename);
  if (e) {
    throw e;
  } else {
    this.loaded = true;
  }
};


Module.prototype._loadScript = function (filename, callback) {
  var self = this;
  cat(filename, function (err, content) {
    debug('cat done');
    if (err) {
      if (callback) callback(err);
    } else {
      var e = self._loadContent(content, filename);
      if (e) {
        if (callback) callback(e);
      } else {
        self._waitChildrenLoad(function () {
          self.loaded = true;
          if (self.onload) self.onload();
          if (callback) callback(null, self.exports);
        });
      }
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


process.exit = function (code) {
  process.emit("exit");
  process.reallyExit(code);
};

var cwd = process.cwd();

// Make process.argv[0] and process.argv[1] into full paths.
if (process.argv[0].indexOf('/') > 0) {
  process.argv[0] = path.join(cwd, process.argv[0]);
}

if (process.argv[1].charAt(0) != "/" && !(/^http:\/\//).exec(process.argv[1])) {
  process.argv[1] = path.join(cwd, process.argv[1]);
}

// Load the main module--the command line argument.
process.mainModule = new Module(".");
process.mainModule.load(process.argv[1], function (err) {
  if (err) throw err;
});

// All our arguments are loaded. We've evaluated all of the scripts. We
// might even have created TCP servers. Now we enter the main eventloop. If
// there are no watchers on the loop (except for the ones that were
// ev_unref'd) then this function exits. As long as there are active
// watchers, it blocks.
process.loop();

process.emit("exit");

});
