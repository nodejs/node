(function () { // annonymous namespace

// deprecation errors

GLOBAL.__module = function () {
  throw new Error("'__module' has been renamed to 'module'");
};

GLOBAL.include = function () {
  throw new Error("include(module) has been removed. Use process.mixin(GLOBAL, require(module)) to get the same effect.");
};

GLOBAL.puts = function () {
  throw new Error("puts() has moved. Use require('sys') to bring it back.");
}

GLOBAL.print = function () {
  throw new Error("print() has moved. Use require('sys') to bring it back.");
}

GLOBAL.p = function () {
  throw new Error("p() has moved. Use require('sys') to bring it back.");
}

process.debug = function () {
  throw new Error("process.debug() has moved. Use require('sys') to bring it back.");
}

process.error = function () {
  throw new Error("process.error() has moved. Use require('sys') to bring it back.");
}

GLOBAL.node = {};

node.createProcess = function () {
  throw new Error("node.createProcess() has been changed to process.createChildProcess() update your code");
};

node.exec = function () {
  throw new Error("process.exec() has moved. Use require('sys') to bring it back.");
};

node.http = {};

node.http.createServer = function () {
  throw new Error("node.http.createServer() has moved. Use require('http') to access it.");
};

node.http.createClient = function () {
  throw new Error("node.http.createClient() has moved. Use require('http') to access it.");
};

node.tcp = {};

node.tcp.createServer = function () {
  throw new Error("node.tcp.createServer() has moved. Use require('tcp') to access it.");
};

node.tcp.createConnection = function () {
  throw new Error("node.tcp.createConnection() has moved. Use require('tcp') to access it.");
};

node.dns = {};

node.dns.createConnection = function () {
  throw new Error("node.dns.createConnection() has moved. Use require('dns') to access it.");
};

node.inherits = function () {
  throw new Error("node.inherits() has moved. Use require('sys') to access it.");
};

process.inherits = function () {
  throw new Error("process.inherits() has moved. Use require('sys') to access it.");
};


process.createChildProcess = function (file, args, env) {
  var child = new process.ChildProcess();
  args = args || [];
  env = env || process.ENV;
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

process.fs.cat = function (path, encoding) {
  var promise = new process.Promise();
  
  encoding = encoding || "utf8"; // default to utf8

  process.fs.open(path, process.O_RDONLY, 0666).addCallback(function (fd) {
    var content = "", pos = 0;

    function readChunk () {
      process.fs.read(fd, 16*1024, pos, encoding).addCallback(function (chunk, bytes_read) {
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
          process.fs.close(fd);
        }
      }).addErrback(function () {
        promise.emitError();
      });
    }
    readChunk();
  }).addErrback(function () {
    promise.emitError(new Error("Could not open " + path));
  });
  return promise;
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
	if ( typeof target !== "object" && !process.isFunction(target) )
		target = {};

	// mixin process itself if only one argument is passed
	if ( length == i ) {
		target = GLOBAL;
		--i;
	}

	for ( ; i < length; i++ )
		// Only deal with non-null/undefined values
		if ( (options = arguments[ i ]) != null )
			// Extend the base object
			for ( var name in options ) {
				var src = target[ name ], copy = options[ name ];

				// Prevent never-ending loop
				if ( target === copy )
					continue;

				// Recurse if we're merging object values
				if ( deep && copy && typeof copy === "object" && !copy.nodeType )
					target[ name ] = process.mixin( deep, 
						// Never move original objects, clone them
						src || ( copy.length != null ? [ ] : { } )
					, copy );

				// Don't bring in undefined values
				else if ( copy !== undefined )
					target[ name ] = copy;

			}

	// Return the modified object
	return target;
};


// Event

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

process.Promise.prototype.timeout = function(timeout) {
  if (timeout === undefined) {
    return this._timeoutDuration;
  }

  this._timeoutDuration = timeout;
  if (this._timer) {
    clearTimeout(this._timer);
    this._timer = null;
  }

  var promiseComplete = false;
  var onComplete = function() {
    promiseComplete = true;
    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = null;
    }
  };

  this
    .addCallback(onComplete)
    .addCancelback(onComplete)
    .addErrback(onComplete);

  var self = this;
  this._timer = setTimeout(function() {
    self._timer = null;
    if (promiseComplete) {
      return;
    }

    self.emitError(new Error('timeout'));
  }, this._timeoutDuration);

  return this;
};

process.Promise.prototype.cancel = function() {
  if(this._cancelled) return;
  this._cancelled = true;

  this._events['success'] = [];
  this._events['error'] = [];

  this.emitCancel();
};

process.Promise.prototype.emitCancel = function() {
  var args = Array.prototype.slice.call(arguments);
  args.unshift('cancel');

  this.emit.apply(this, args);
};

process.Promise.prototype.addCallback = function (listener) {
  this.addListener("success", listener);
  return this;
};

process.Promise.prototype.addErrback = function (listener) {
  this.addListener("error", listener);
  return this;
};

process.Promise.prototype.addCancelback = function (listener) {
  this.addListener("cancel", listener);
  return this;
};

process.Promise.prototype.wait = function () {
  var ret;
  var had_error = false;
  this.addCallback(function () {
        if (arguments.length == 1) {
          ret = arguments[0];
        } else if (arguments.length > 1) {
          ret = [];
          for (var i = 0; i < arguments.length; i++) {
            ret.push(arguments[i]);
          }
        }
      })
      .addErrback(function (arg) {
        had_error = true;
        ret = arg;
      })
      .block();

  if (had_error) {
    if (ret) {
      throw ret;
    } else {
      throw new Error("Promise completed with error (No arguments given.)");
    }
  }
  return ret;
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



// Timers

GLOBAL.setTimeout = function (callback, after) {
  var timer = new process.Timer();
  timer.addListener("timeout", callback);
  timer.start(after, 0);
  return timer;
}

GLOBAL.setInterval = function (callback, repeat) {
  var timer = new process.Timer();
  timer.addListener("timeout", callback);
  timer.start(repeat, repeat);
  return timer;
}

GLOBAL.clearTimeout = function (timer) {
  timer.stop();
}

GLOBAL.clearInterval = GLOBAL.clearTimeout;




// Modules

var debugLevel = 0;
if ("NODE_DEBUG" in process.ENV) debugLevel = 1;

function debug (x) {
  if (debugLevel > 0) {
    process.stdio.writeError(x + "\n");
  }
}


// private constructor
function Module (id, parent) {
  this.id = id;
  this.exports = {};
  this.parent = parent;

  this.filename = null;
  this.loaded = false;
  this.loadPromise = null;
  this.exited = false;
  this.children = [];
};

var moduleCache = {};

function createModule (id, parent) {
  if (id in moduleCache) {
    debug("found " + JSON.stringify(id) + " in cache");
    return moduleCache[id];
  }
  debug("didn't found " + JSON.stringify(id) + " in cache. creating new module");
  var m = new Module(id, parent);
  moduleCache[id] = m;
  return m;
};

function createInternalModule (id, constructor) {
  var m = createModule(id);
  constructor(m.exports);
  m.loaded = true;
  return m;
};

var pathModule = createInternalModule("path", function (exports) {
  exports.join = function () {
    var joined = "";
    for (var i = 0; i < arguments.length; i++) {
      var part = arguments[i].toString();

      /* Some logic to shorten paths */
      if (part === ".") continue;
      while (/^\.\//.exec(part)) part = part.replace(/^\.\//, "");

      if (i === 0) {
        part = part.replace(/\/*$/, "/");
      } else if (i === arguments.length - 1) {
        part = part.replace(/^\/*/, "");
      } else {
        part = part.replace(/^\/*/, "").replace(/\/*$/, "/");
      }
      joined += part;
    }
    return joined;
  };

  exports.dirname = function (path) {
    if (path.charAt(0) !== "/") path = "./" + path;
    var parts = path.split("/");
    return parts.slice(0, parts.length-1).join("/");
  };

  exports.filename = function (path) {
    if (path.charAt(0) !== "/") path = "./" + path;
    var parts = path.split("/");
    return parts[parts.length-1];
  };

  exports.exists = function (path, callback) {
    var p = process.fs.stat(path);
    p.addCallback(function () { callback(true); });
    p.addErrback(function () { callback(false); });
  };
});

var path = pathModule.exports;


process.paths = [ path.join(process.installPrefix, "lib/node/libraries")
               ];
 
if (process.ENV["HOME"]) {
  process.paths.unshift(path.join(process.ENV["HOME"], ".node_libraries"));
}

if (process.ENV["NODE_PATH"]) {
  process.paths = process.ENV["NODE_PATH"].split(":").concat(process.paths);
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
    path.join(dir, id, "index.addon"),
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
    })
  };
  searchLocations();
}

function loadModule (request, parent) {
  // This is the promise which is actually returned from require.async()
  var loadPromise = new process.Promise();

  debug("loadModule REQUEST  " + JSON.stringify(request) + " parent: " + JSON.stringify(parent));

  var id, paths;
  if (request.charAt(0) == "." && (request.charAt(1) == "/" || request.charAt(1) == ".")) {
    // Relative request
    id = path.join(path.dirname(parent.id), request);
    paths = [path.dirname(parent.filename)];
  } else {
    id = request;
    paths = process.paths;
  }

  if (id in moduleCache) {
    debug("found  " + JSON.stringify(id) + " in cache");
    // In cache
    var module = moduleCache[id];
    setTimeout(function () {
      loadPromise.emitSuccess(module.exports);
    }, 0);
  } else {
    debug("looking for " + JSON.stringify(id) + " in " + JSON.stringify(paths));
    // Not in cache
    findModulePath(request, paths, function (filename) {
      if (!filename) {
        loadPromise.emitError(new Error("Cannot find module '" + request + "'"));
      } else {
        var module = createModule(id, parent);
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
  setTimeout(function () {
    self.loaded = true;
    process.dlopen(filename, self.exports); // FIXME synchronus
    loadPromise.emitSuccess(self.exports);
  }, 0);
};

function cat (id, loadPromise) {
  var promise;

  if (id.match(/^http:\/\//)) {
    promise = new process.Promise();
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
    promise = process.fs.cat(id);
  }

  return promise;
}

Module.prototype.loadScript = function (filename, loadPromise) {
  var self = this;
  var catPromise = cat(filename, loadPromise);

  catPromise.addErrback(function () {
    loadPromise.emitError(new Error("Error reading " + filename));
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
    var wrapper = "var __wrap__ = function (exports, require, module, __filename) { " 
                + content 
                + "\n}; __wrap__;";
    var compiledWrapper = process.compile(wrapper, filename);

    compiledWrapper.apply(self.exports, [self.exports, require, self, filename]);

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

// Make process.ARGV[0] and process.ARGV[1] into full paths.
if (process.ARGV[0].charAt(0) != "/") {
  process.ARGV[0] = path.join(cwd, process.ARGV[0]);
}

if (process.ARGV[1].charAt(0) != "/" && !/^http:\/\//.exec(process.ARGV[1])) {
  process.ARGV[1] = path.join(cwd, process.ARGV[1]);
}

// Load the main module--the command line argument.
process.mainModule = createModule(".");
var loadPromise = new process.Promise();
process.mainModule.load(process.ARGV[1], loadPromise);

// All our arguments are loaded. We've evaluated all of the scripts. We
// might even have created TCP servers. Now we enter the main eventloop. If
// there are no watchers on the loop (except for the ones that were
// ev_unref'd) then this function exits. As long as there are active
// watchers, it blocks.
process.loop();

process.emit("exit");

}()); // end annonymous namespace
