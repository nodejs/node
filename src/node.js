process.tcp.createServer = function () {
  throw new Error("process.tcp.createServer() has moved. Use require('/tcp.js') to access it.");
};

process.createProcess = function () {
  throw "process.createProcess() has been changed to process.createChildProcess() update your code";
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

process.exec = function () {
  throw new Error("process.exec() has moved. Use require('/sys.js') to bring it back.");
}

process.http.createServer = function () {
  throw new Error("process.http.createServer() has moved. Use require('/http.js') to access it.");
}

process.http.createClient = function () {
  throw new Error("process.http.createClient() has moved. Use require('/http.js') to access it.");
}

process.tcp.createConnection = function (port, host) {
  throw new Error("process.tcp.createConnection() has moved. Use require('/tcp.js') to access it.");
};

include = function () {
  throw new Error("include() has been removed. Use process.mixin(process, require(file)) to get the same effect.");
}

/* From jQuery.extend in the jQuery JavaScript Library v1.3.2
 * Copyright (c) 2009 John Resig
 * Dual licensed under the MIT and GPL licenses.
 * http://docs.jquery.com/License
 */
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

// Signal Handlers

(function () { // anonymous namespace

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

})(); // anonymous namespace

// Timers

function setTimeout (callback, after) {
  var timer = new process.Timer();
  timer.addListener("timeout", callback);
  timer.start(after, 0);
  return timer;
}

function setInterval (callback, repeat) {
  var timer = new process.Timer();
  timer.addListener("timeout", callback);
  timer.start(repeat, repeat);
  return timer;
}

function clearTimeout (timer) {
  timer.stop();
}

clearInterval = clearTimeout;

// Module

process.libraryPaths = [ process.path.join(process.ENV["HOME"], ".node_libraries")
                       , process.path.join(process.installPrefix, "lib/node/libraries")
                       , "/" 
                       ];

if (process.ENV["NODE_LIBRARY_PATHS"]) {
  process.libraryPaths =
    process.ENV["NODE_LIBRARY_PATHS"].split(":").concat(process.libraryPaths);
}

process.Module = function (filename, parent) {
  process.assert(filename.charAt(0) == "/");
  this.filename = filename;
  this.exports = {};
  this.parent = parent;

  this.loaded = false;
  this.loadPromise = null;
  this.exited = false;
  this.children = [];
};

process.Module.cache = {};

(function () {
  function retrieveFromCache (loadPromise, fullPath, parent) {
    var module;
    if (fullPath in process.Module.cache) {
      module = process.Module.cache[fullPath];
      setTimeout(function () {
        loadPromise.emitSuccess(module.exports);
      }, 0);
    } else {
      module = new process.Module(fullPath, parent);
      process.Module.cache[fullPath] = module;
      module.load(loadPromise);
    }
  }

  function findPath (path, dirs, callback) {
    process.assert(path.charAt(0) == "/");
    process.assert(dirs.constructor == Array);

    if (dirs.length == 0) {
      callback();
    } else {
      var dir = dirs[0];
      var rest = dirs.slice(1, dirs.length);

      var fullPath = process.path.join(dir, path);
      process.fs.exists(fullPath, function (doesExist) {
        if (doesExist) {
          callback(fullPath);
        } else {
          findPath(path, rest, callback);  
        }
      });
    }
  }

  process.loadModule = function (requestedPath, exports, parent) {
    var loadPromise = new process.Promise();

    // On success copy the loaded properties into the exports
    loadPromise.addCallback(function (t) {
      for (var prop in t) {
        if (t.hasOwnProperty(prop)) exports[prop] = t[prop];
      }
    });

    loadPromise.addErrback(function (e) {
      process.stdio.writeError(e.message + "\n");
      process.exit(1);
    });

    if (!parent) {
      // root module
      process.assert(requestedPath.charAt(0) == "/");
      retrieveFromCache(loadPromise, requestedPath);

    } else {
      if (requestedPath.charAt(0) == "/") {
        // Need to find the module in process.libraryPaths
        findPath(requestedPath, process.libraryPaths, function (fullPath) {
          if (fullPath) {
            retrieveFromCache(loadPromise, fullPath, parent);
          } else {
            loadPromise.emitError(new Error("Cannot find module '" + requestedPath + "'"));
          }
        });

      } else {
        // Relative file load
        var fullPath = process.path.join(process.path.dirname(parent.filename),
            requestedPath);
        retrieveFromCache(loadPromise, fullPath, parent);
      }
    }

    return loadPromise;
  };
}());

process.Module.prototype.load = function (loadPromise) {
  if (this.loaded) {
    loadPromise.emitError(new Error("Module '" + self.filename + "' is already loaded."));
    return;
  }
  process.assert(!process.loadPromise);
  this.loadPromise = loadPromise;

  if (this.filename.match(/\.node$/)) {
    this.loadObject(loadPromise);
  } else {
    this.loadScript(loadPromise);
  }
};

process.Module.prototype.loadObject = function (loadPromise) {
  var self = this;
  // XXX Not yet supporting loading from HTTP. would need to download the
  // file, store it to tmp then run dlopen on it. 
  process.fs.exists(self.filename, function (does_exist) {
    if (does_exist) {
      self.loaded = true;
      process.dlopen(self.filename, self.exports); // FIXME synchronus
      loadPromise.emitSuccess(self.exports);
    } else {
      loadPromise.emitError(new Error("Error reading " + self.filename));
    }
  });
};

process.Module.prototype.loadScript = function (loadPromise) {
  var self = this;
  var catPromise = process.cat(self.filename);

  catPromise.addErrback(function () {
    loadPromise.emitError(new Error("Error reading " + self.filename));
  });

  catPromise.addCallback(function (content) {
    // remove shebang
    content = content.replace(/^\#\!.*/, '');

    function requireAsync (url) {
      return self.newChild(url);
    }

    function require (url) {
      return requireAsync(url).wait();
    }

    require.paths = process.libraryPaths;
    require.async = requireAsync;

    // create wrapper function
    var wrapper = "var __wrap__ = function (__module, __filename, exports, require) { " 
                + content 
                + "\n}; __wrap__;";
    var compiled_wrapper = process.compile(wrapper, self.filename);

    compiled_wrapper.apply(self.exports, [self, self.filename, self.exports, require]);

    self.waitChildrenLoad(function () {
      self.loaded = true;
      loadPromise.emitSuccess(self.exports);
    });
  });
};

process.Module.prototype.newChild = function (path) {
  return process.loadModule(path, {}, this);
};

process.Module.prototype.waitChildrenLoad = function (callback) {
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

(function () {
  var cwd = process.cwd();

  // Make process.ARGV[0] and process.ARGV[1] into full paths.
  if (process.ARGV[0].charAt(0) != "/") {
    process.ARGV[0] = process.path.join(cwd, process.ARGV[0]);
  }

  if (process.ARGV[1].charAt(0) != "/") {
    process.ARGV[1] = process.path.join(cwd, process.ARGV[1]);
  }

  // Load the root module--the command line argument.
  process.loadModule(process.ARGV[1], process);
}());
