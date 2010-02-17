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
GLOBAL.include = removed("include(module) has been removed. Use process.mixin(GLOBAL, require(module)) to get the same effect.");
GLOBAL.puts = removed("puts() has moved. Use require('sys') to bring it back.");
GLOBAL.print = removed("print() has moved. Use require('sys') to bring it back.");
GLOBAL.p = removed("p() has moved. Use require('sys') to bring it back.");
process.debug = removed("process.debug() has moved. Use require('sys') to bring it back.");
process.error = removed("process.error() has moved. Use require('sys') to bring it back.");

GLOBAL.node = {};

node.createProcess = removed("node.createProcess() has been changed to process.createChildProcess() update your code");
node.exec = removed("process.exec() has moved. Use require('sys') to bring it back.");
node.inherits = removed("node.inherits() has moved. Use require('sys') to access it.");

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
  this.loadPromise = null;
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


process.inherits = function (ctor, superCtor) {
  var tempCtor = function(){};
  tempCtor.prototype = superCtor.prototype;
  ctor.super_ = superCtor;
  ctor.prototype = new tempCtor();
  ctor.prototype.constructor = ctor;
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
process.mixin = function() {
  // copy reference to target object
  var target = arguments[0] || {}, i = 1, length = arguments.length, deep = false, options;

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
    if ( (options = arguments[ i ]) != null ) {
      // Extend the base object
      for ( var name in options ) {
        var src = target[ name ], copy = options[ name ];

        // Prevent never-ending loop
        if ( target === copy )
          continue;

        // Recurse if we're merging object values
        if ( deep && copy && typeof copy === "object" ) {
          target[ name ] = process.mixin( deep,
            // Never move original objects, clone them
            src || ( copy.length != null ? [ ] : { } )
          , copy );

        // Don't bring in undefined values
        } else {
          target[ name ] = copy;
        }
      }
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
    if (listener instanceof Function) {
      if (!this._events) this._events = {};
      if (!this._events.hasOwnProperty(type)) this._events[type] = [];
      // To avoid recursion in the case that type == "newListeners"! Before
      // adding it to the listeners, first emit "newListeners".
      this.emit("newListener", type, listener);
      this._events[type].push(listener);
    }
    return this;
  };

  process.EventEmitter.prototype.removeListener = function (type, listener) {
    if (listener instanceof Function) {
      // does not use listeners(), so no side effect of creating _events[type]
      if (!this._events || !this._events.hasOwnProperty(type)) return;
      var list = this._events[type];
      if (list.indexOf(listener) < 0) return;
      list.splice(list.indexOf(listener), 1);
    }
    return this;
  };

  process.EventEmitter.prototype.listeners = function (type) {
    if (!this._events) this._events = {};
    if (!this._events.hasOwnProperty(type)) this._events[type] = [];
    return this._events[type];
  };

  exports.Promise = function () {
    exports.EventEmitter.call(this);
    this._blocking = false;
    this.hasFired = false;
    this._values = undefined;
  };
  process.inherits(exports.Promise, exports.EventEmitter);

  process.Promise = exports.Promise;

  exports.Promise.prototype.timeout = function(timeout) {
    if (!timeout) {
      return this._timeoutDuration;
    }
    
    this._timeoutDuration = timeout;
    
    if (this.hasFired) return;
    this._clearTimeout();

    var self = this;
    this._timer = setTimeout(function() {
      self._timer = null;
      if (self.hasFired) {
        return;
      }

      self.emitError(new Error('timeout'));
    }, timeout);

    return this;
  };
  
  exports.Promise.prototype._clearTimeout = function() {
    if (!this._timer) return;
    
    clearTimeout(this._timer);
    this._timer = null;
  }

  exports.Promise.prototype.emitSuccess = function() {
    if (this.hasFired) return;
    this.hasFired = 'success';
    this._clearTimeout();

    this._values = Array.prototype.slice.call(arguments);
    this.emit.apply(this, ['success'].concat(this._values));
  };

  exports.Promise.prototype.emitError = function() {
    if (this.hasFired) return;
    this.hasFired = 'error';
    this._clearTimeout();

    this._values = Array.prototype.slice.call(arguments);
    this.emit.apply(this, ['error'].concat(this._values));

    if (this.listeners('error').length == 0) {
      var self = this;
      process.nextTick(function() {
        if (self.listeners('error').length == 0) {
          throw (self._values[0] instanceof Error)
            ? self._values[0]
            : new Error('Unhandled emitError: '+JSON.stringify(self._values));
        }
      });
    }
  };

  exports.Promise.prototype.addCallback = function (listener) {
    if (this.hasFired === 'success') {
      listener.apply(this, this._values);
    }

    return this.addListener("success", listener);
  };

  exports.Promise.prototype.addErrback = function (listener) {
    if (this.hasFired === 'error') {
      listener.apply(this, this._values);
    }

    return this.addListener("error", listener);
  };

  /* Poor Man's coroutines */
  var coroutineStack = [];

  exports.Promise.prototype._destack = function () {
    this._blocking = false;

    while (coroutineStack.length > 0 &&
           !coroutineStack[coroutineStack.length-1]._blocking)
    {
      coroutineStack.pop();
      process.unloop("one");
    }
  };

  exports.Promise.prototype.wait = function () {
    var self = this;
    var ret;
    var hadError = false;

    if (this.hasFired) {
      ret = (this._values.length == 1)
          ? this._values[0]
          : this.values;

      if (this.hasFired == 'success') {
        return ret;
      } else if (this.hasFired == 'error') {
        throw ret;
      }
    }

    self.addCallback(function () {
      if (arguments.length == 1) {
        ret = arguments[0];
      } else if (arguments.length > 1) {
        ret = Array.prototype.slice.call(arguments);
      }
      self._destack();
    });

    self.addErrback(function (arg) {
      hadError = true;
      ret = arg;
      self._destack();
    });

    coroutineStack.push(self);
    if (coroutineStack.length > 10) {
      process.stdio.writeError("WARNING: promise.wait() is being called too often.\n");
    }
    self._blocking = true;

    process.loop();

    process.assert(self._blocking == false);

    if (hadError) {
      if (ret) {
        throw ret;
      } else {
        throw new Error("Promise completed with error (No arguments given.)");
      }
    }
    return ret;
  };
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


// Stat Change Watchers

var statWatchers = {};

process.watchFile = function (filename) {
  var stat;
  var options;
  var listener;

  if ("object" == typeof arguments[1]) {
    options = arguments[1];
    listener = arguments[2];
  } else {
    options = {};
    listener = arguments[1];
  }

  if (options.persistent === undefined) options.persistent = true;
  if (options.interval === undefined) options.interval = 0;

  if (filename in statWatchers) {
    stat = statWatchers[filename];
  } else {
    statWatchers[filename] = new process.Stat();
    stat = statWatchers[filename];
    stat.start(filename, options.persistent, options.interval);
  }
  stat.addListener("change", listener);
  return stat;
};

process.unwatchFile = function (filename) {
  if (filename in statWatchers) {
    stat = statWatchers[filename];
    stat.stop();
    statWatchers[filename] = undefined;
  }
};

process.Stats.prototype._checkModeProperty = function (property) {
  return ((this.mode & property) === property);
};

process.Stats.prototype.isDirectory = function () {
  return this._checkModeProperty(process.S_IFDIR);
};

process.Stats.prototype.isFile = function () {
  return this._checkModeProperty(process.S_IFREG);
};

process.Stats.prototype.isBlockDevice = function () {
  return this._checkModeProperty(process.S_IFBLK);
};

process.Stats.prototype.isCharacterDevice = function () {
  return this._checkModeProperty(process.S_IFCHR);
};

process.Stats.prototype.isSymbolicLink = function () {
  return this._checkModeProperty(process.S_IFLNK);
};

process.Stats.prototype.isFIFO = function () {
  return this._checkModeProperty(process.S_IFIFO);
};

process.Stats.prototype.isSocket = function () {
  return this._checkModeProperty(process.S_IFSOCK);
};



// Timers
function addTimerListener (callback) {
  var timer = this;
  // Special case the no param case to avoid the extra object creation.
  if (arguments.length > 2) {
    var args = Array.prototype.slice.call(arguments, 2);
    timer.addListener("timeout", function(){
      callback.apply(timer, args);
    });
  } else {
    timer.addListener("timeout", callback);
  }
}

GLOBAL.setTimeout = function (callback, after) {
  var timer = new process.Timer();
  addTimerListener.apply(timer, arguments);
  timer.start(after, 0);
  return timer;
};

GLOBAL.setInterval = function (callback, repeat) {
  var timer = new process.Timer();
  addTimerListener.apply(timer, arguments);
  timer.start(repeat, repeat);
  return timer;
};

GLOBAL.clearTimeout = function (timer) {
  if (timer instanceof process.Timer) {
    timer.stop();
  }
};

GLOBAL.clearInterval = GLOBAL.clearTimeout;




// Modules

var debugLevel = 0;
if ("NODE_DEBUG" in process.env) debugLevel = 1;

function debug (x) {
  if (debugLevel > 0) {
    process.stdio.writeError(x + "\n");
  }
}



var fsModule = createInternalModule("fs", function (exports) {
  exports.Stats = process.Stats;

  function callback (promise) {
    return function (error) {
      if (error) {
        promise.emitError.apply(promise, arguments);
      } else {
        promise.emitSuccess.apply(promise,
                                  Array.prototype.slice.call(arguments, 1));
      }
    };
  }
  
  // Used by fs.open and friends
  function stringToFlags(flag) {
    // Only mess with strings
    if (typeof flag !== 'string') {
      return flag;
    }
    switch (flag) {
      case "r": return process.O_RDONLY;
      case "r+": return process.O_RDWR;
      case "w": return process.O_CREAT | process.O_TRUNC | process.O_WRONLY;
      case "w+": return process.O_CREAT | process.O_TRUNC | process.O_RDWR;
      case "a": return process.O_APPEND | process.O_CREAT | process.O_WRONLY; 
      case "a+": return process.O_APPEND | process.O_CREAT | process.O_RDWR;
      default: throw new Error("Unknown file open flag: " + flag);
    }
  }

  // Yes, the follow could be easily DRYed up but I provide the explicit
  // list to make the arguments clear.

  exports.close = function (fd) {
    var promise = new events.Promise();
    process.fs.close(fd, callback(promise));
    return promise;
  };

  exports.closeSync = function (fd) {
    return process.fs.close(fd);
  };

  exports.open = function (path, flags, mode) {
    if (mode === undefined) { mode = 0666; }
    var promise = new events.Promise();
    process.fs.open(path, stringToFlags(flags), mode, callback(promise));
    return promise;
  };

  exports.openSync = function (path, flags, mode) {
    if (mode === undefined) { mode = 0666; }
    return process.fs.open(path, stringToFlags(flags), mode);
  };

  exports.read = function (fd, length, position, encoding) {
    var promise = new events.Promise();
    encoding = encoding || "binary";
    process.fs.read(fd, length, position, encoding, callback(promise));
    return promise;
  };

  exports.readSync = function (fd, length, position, encoding) {
    encoding = encoding || "binary";
    return process.fs.read(fd, length, position, encoding);
  };

  exports.write = function (fd, data, position, encoding) {
    var promise = new events.Promise();
    encoding = encoding || "binary";
    process.fs.write(fd, data, position, encoding, callback(promise));
    return promise;
  };

  exports.writeSync = function (fd, data, position, encoding) {
    encoding = encoding || "binary";
    return process.fs.write(fd, data, position, encoding);
  };

  exports.rename = function (oldPath, newPath) {
    var promise = new events.Promise();
    process.fs.rename(oldPath, newPath, callback(promise));
    return promise;
  };

  exports.renameSync = function (oldPath, newPath) {
    return process.fs.rename(oldPath, newPath);
  };

  exports.truncate = function (fd, len) {
    var promise = new events.Promise();
    process.fs.truncate(fd, len, callback(promise));
    return promise;
  };

  exports.truncateSync = function (fd, len) {
    return process.fs.truncate(fd, len);
  };

  exports.rmdir = function (path) {
    var promise = new events.Promise();
    process.fs.rmdir(path, callback(promise));
    return promise;
  };

  exports.rmdirSync = function (path) {
    return process.fs.rmdir(path);
  };

  exports.mkdir = function (path, mode) {
    var promise = new events.Promise();
    process.fs.mkdir(path, mode, callback(promise));
    return promise;
  };

  exports.mkdirSync = function (path, mode) {
    return process.fs.mkdir(path, mode);
  };

  exports.sendfile = function (outFd, inFd, inOffset, length) {
    var promise = new events.Promise();
    process.fs.sendfile(outFd, inFd, inOffset, length, callback(promise));
    return promise;
  };

  exports.sendfileSync = function (outFd, inFd, inOffset, length) {
    return process.fs.sendfile(outFd, inFd, inOffset, length);
  };

  exports.readdir = function (path) {
    var promise = new events.Promise();
    process.fs.readdir(path, callback(promise));
    return promise;
  };

  exports.readdirSync = function (path) {
    return process.fs.readdir(path);
  };

  exports.stat = function (path) {
    var promise = new events.Promise();
    process.fs.stat(path, callback(promise));
    return promise;
  };

  exports.statSync = function (path) {
    return process.fs.stat(path);
  };

  exports.unlink = function (path) {
    var promise = new events.Promise();
    process.fs.unlink(path, callback(promise));
    return promise;
  };

  exports.unlinkSync = function (path) {
    return process.fs.unlink(path);
  };

  exports.writeFile = function (path, data, encoding) {
    var promise = new events.Promise();
    encoding = encoding || "utf8"; // default to utf8

    fs.open(path, "w")
      .addCallback(function (fd) {
        function doWrite (_data) {
          fs.write(fd, _data, 0, encoding)
            .addErrback(function () {
              fs.close(fd);
              promise.emitError();
            })
            .addCallback(function (written) {
              if (written === _data.length) {
                fs.close(fd);
                promise.emitSuccess();
              } else {
                doWrite(_data.slice(written));
              }
            });
        }
        doWrite(data);
      })
      .addErrback(function () {
        promise.emitError();
      });

    return promise;
    
  };
  
  exports.writeFileSync = function (path, data, encoding) {
    encoding = encoding || "utf8"; // default to utf8
    var fd = exports.openSync(path, "w");
    return process.fs.write(fd, data, 0, encoding);
  };
  
  
  exports.cat = function () {
    throw new Error("fs.cat is deprecated. Please use fs.readFile instead.");
  };

  exports.readFile = function (path, encoding) {
    var promise = new events.Promise();

    encoding = encoding || "utf8"; // default to utf8

    exports.open(path, "r").addCallback(function (fd) {
      var content = "", pos = 0;

      function readChunk () {
        exports.read(fd, 16*1024, pos, encoding).addCallback(function (chunk, bytes_read) {
          if (chunk) {
            if (chunk.constructor === String) {
              content += chunk;
            } else {
              content = content.concat(chunk);
            }

            pos += bytes_read;
            readChunk();
          } else {
            promise.emitSuccess(content);
            exports.close(fd);
          }
        }).addErrback(function () {
          promise.emitError.apply(promise, arguments);
        });
      }
      readChunk();
    }).addErrback(function () {
      promise.emitError.apply(promise, arguments);
    });
    return promise;
  };

  exports.catSync = function () {
    throw new Error("fs.catSync is deprecated. Please use fs.readFileSync instead.");
  };

  exports.readFileSync = function (path, encoding) {
    encoding = encoding || "utf8"; // default to utf8

    var
      fd = exports.openSync(path, "r"),
      content = '',
      pos = 0,
      r;

    while ((r = exports.readSync(fd, 16*1024, pos, encoding)) && r[0]) {
      content += r[0];
      pos += r[1]
    }

    return content;
  };
});

var fs = fsModule.exports;


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
    var p = fs.stat(path);
    p.addCallback(function () { callback(true); });
    p.addErrback(function () { callback(false); });
  };
});

var path = pathModule.exports;


process.paths = [ path.join(process.installPrefix, "lib/node/libraries")
               ];

if (process.env["HOME"]) {
  process.paths.unshift(path.join(process.env["HOME"], ".node_libraries"));
}

if (process.env["NODE_PATH"]) {
  process.paths = process.env["NODE_PATH"].split(":").concat(process.paths);
}


function findModulePath (id, dirs, callback) {
  process.assert(dirs.constructor == Array);

  if (/^https?:\/\//.exec(id)) {
    callback(id);
    return;
  }

  if (/\.(js|node)$/.exec(id)) {
    throw new Error("No longer accepting filename extension in module names");
  }

  if (dirs.length == 0) {
    callback();
    return;
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
    path.join(dir, id, "index.addon")
  ];

  var searchLocations = function() {
    var location = locations.shift();
    if (location === undefined) {
      findModulePath(id, rest, callback);
      return;
    }

    path.exists(location, function (found) {
      if (found) {
        callback(location);
        return;
      }
      searchLocations();
    });
  };
  searchLocations();
}

function resolveModulePath(request, parent) {

  var id, paths;
  if (request.charAt(0) == "." && (request.charAt(1) == "/" || request.charAt(1) == ".")) {
    // Relative request
    var parentIdPath = path.dirname(parent.id +
      (path.basename(parent.filename).match(/^index\.(js|addon)$/) ? "/" : ""));
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

function loadModule (request, parent) {
  var
    // The promise returned from require.async()
    loadPromise = new events.Promise(),
    resolvedModule = resolveModulePath(request, parent),
    id = resolvedModule[0],
    paths = resolvedModule[1];

  // debug("loadModule REQUEST  " + (request) + " parent: " + JSON.stringify(parent));

  var cachedModule = internalModuleCache[id] || parent.moduleCache[id];
  if (cachedModule) {
    debug("found  " + JSON.stringify(id) + " in cache");
    process.nextTick(function() {
      loadPromise.emitSuccess(cachedModule.exports);
    });
   } else {
    debug("looking for " + JSON.stringify(id) + " in " + JSON.stringify(paths));
    // Not in cache
    findModulePath(request, paths, function (filename) {
      if (!filename) {
        loadPromise.emitError(new Error("Cannot find module '" + request + "'"));
      } else {
        var module = new Module(id, parent);
        module.load(filename, loadPromise);
      }
    });
  }

  return loadPromise;
};

Module.prototype.load = function (filename, loadPromise) {
  debug("load " + JSON.stringify(filename) + " for module " + JSON.stringify(this.id));

  process.assert(!this.loaded);
  process.assert(!this.loadPromise);

  this.loadPromise = loadPromise;
  this.filename = filename;

  if (filename.match(/\.node$/)) {
    this.loadObject(filename, loadPromise);
  } else {
    this.loadScript(filename, loadPromise);
  }
};

Module.prototype.loadObject = function (filename, loadPromise) {
  var self = this;
  // XXX Not yet supporting loading from HTTP. would need to download the
  // file, store it to tmp then run dlopen on it.
  process.nextTick(function () {
    self.loaded = true;
    process.dlopen(filename, self.exports); // FIXME synchronus
    loadPromise.emitSuccess(self.exports);
  });
};

function cat (id, loadPromise) {
  var promise;

  debug(id);

  if (id.match(/^http:\/\//)) {
    promise = new events.Promise();
    loadModule('http', process.mainModule)
      .addCallback(function(http) {
        http.cat(id)
          .addCallback(function(content) {
            promise.emitSuccess(content);
          })
          .addErrback(function() {
            promise.emitError.apply(null, arguments);
          });
      })
      .addErrback(function() {
        loadPromise.emitError(new Error("could not load core module \"http\""));
      });
  } else {
    promise = fs.readFile(id);
  }

  return promise;
}

Module.prototype.loadScript = function (filename, loadPromise) {
  var self = this;
  var catPromise = cat(filename, loadPromise);

  catPromise.addErrback(function () {
    loadPromise.emitError(new Error("Cannot read " + filename));
  });

  catPromise.addCallback(function (content) {
    // remove shebang
    content = content.replace(/^\#\!.*/, '');

    function requireAsync (url) {
      return loadModule(url, self); // new child
    }

    function require (url) {
      return requireAsync(url).wait();
    }

    require.paths = process.paths;
    require.async = requireAsync;
    require.main = process.mainModule;
    // create wrapper function
    var wrapper = "(function (exports, require, module, __filename, __dirname) { "
                + content
                + "\n});";

    try {
      var compiledWrapper = process.compile(wrapper, filename);
      compiledWrapper.apply(self.exports, [self.exports, require, self, filename, path.dirname(filename)]);
    } catch (e) {
      loadPromise.emitError(e);
      return;
    }

    self.waitChildrenLoad(function () {
      self.loaded = true;
      loadPromise.emitSuccess(self.exports);
    });
  });
};

Module.prototype.waitChildrenLoad = function (callback) {
  var nloaded = 0;
  var children = this.children;
  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    if (child.loaded) {
      nloaded++;
    } else {
      child.loadPromise.addCallback(function () {
        nloaded++;
        if (children.length == nloaded && callback) callback();
      });
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
var loadPromise = new events.Promise();
process.mainModule.load(process.argv[1], loadPromise);

// All our arguments are loaded. We've evaluated all of the scripts. We
// might even have created TCP servers. Now we enter the main eventloop. If
// there are no watchers on the loop (except for the ones that were
// ev_unref'd) then this function exits. As long as there are active
// watchers, it blocks.
process.loop();

process.emit("exit");

})
