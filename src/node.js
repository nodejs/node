node.tcp.createServer = function () {
  throw new Error("node.tcp.createServer() has moved. Use require('/tcp.js') to access it.");
};

node.createProcess = function () {
  throw "node.createProcess() has been changed to node.createChildProcess() update your code";
};

node.createChildProcess = function (file, args, env) {
  var child = new node.ChildProcess();
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

node.exec = function () {
  throw new Error("node.exec() has moved. Use require('/sys.js') to bring it back.");
}

node.http.createServer = function () {
  throw new Error("node.http.createServer() has moved. Use require('/http.js') to access it.");
}

node.http.createClient = function () {
  throw new Error("node.http.createClient() has moved. Use require('/http.js') to access it.");
}

node.tcp.createConnection = function (port, host) {
  throw new Error("node.tcp.createConnection() has moved. Use require('/tcp.js') to access it.");
};

include = function () {
  throw new Error("include() has been removed. Use node.mixin(process, require(file)) to get the same effect.");
}

/* From jQuery.extend in the jQuery JavaScript Library v1.3.2
 * Copyright (c) 2009 John Resig
 * Dual licensed under the MIT and GPL licenses.
 * http://docs.jquery.com/License
 */
node.mixin = function() {
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
	if ( typeof target !== "object" && !node.isFunction(target) )
		target = {};

	// mixin process itself if only one argument is passed
	if ( length == i ) {
		target = process;
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
					target[ name ] = node.mixin( deep, 
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

// Timers

function setTimeout (callback, after) {
  var timer = new node.Timer();
  timer.addListener("timeout", callback);
  timer.start(after, 0);
  return timer;
}

function setInterval (callback, repeat) {
  var timer = new node.Timer();
  timer.addListener("timeout", callback);
  timer.start(repeat, repeat);
  return timer;
}

function clearTimeout (timer) {
  timer.stop();
}

clearInterval = clearTimeout;

// Module

node.libraryPaths = [ node.path.join(ENV["HOME"], ".node_libraries")
                    , node.path.join(node.installPrefix, "lib/node/libraries")
                    , "/" 
                    ];

if (ENV["NODE_LIBRARY_PATHS"]) {
  node.libraryPaths =
    ENV["NODE_LIBRARY_PATHS"].split(":").concat(node.libraryPaths);
}

node.Module = function (filename, parent) {
  node.assert(filename.charAt(0) == "/");
  this.filename = filename;
  this.exports = {};
  this.parent = parent;

  this.loaded = false;
  this.loadPromise = null;
  this.exited = false;
  this.children = [];
};

node.Module.cache = {};

(function () {
  function retrieveFromCache (loadPromise, fullPath, parent) {
    var module;
    if (fullPath in node.Module.cache) {
      module = node.Module.cache[fullPath];
      setTimeout(function () {
        loadPromise.emitSuccess(module.exports);
      }, 0);
    } else {
      module = new node.Module(fullPath, parent);
      node.Module.cache[fullPath] = module;
      module.load(loadPromise);
    }
  }

  function findPath (path, dirs, callback) {
    node.assert(path.charAt(0) == "/");
    node.assert(dirs.constructor == Array);

    if (dirs.length == 0) {
      callback();
    } else {
      var dir = dirs[0];
      var rest = dirs.slice(1, dirs.length);

      var fullPath = node.path.join(dir, path);
      node.fs.exists(fullPath, function (doesExist) {
        if (doesExist) {
          callback(fullPath);
        } else {
          findPath(path, rest, callback);  
        }
      });
    }
  }

  node.loadModule = function (requestedPath, exports, parent) {
    var loadPromise = new node.Promise();

    // On success copy the loaded properties into the exports
    loadPromise.addCallback(function (t) {
      for (var prop in t) {
        if (t.hasOwnProperty(prop)) exports[prop] = t[prop];
      }
    });

    loadPromise.addErrback(function (e) {
      node.stdio.writeError(e.message + "\n");
      process.exit(1);
    });

    if (!parent) {
      // root module
      node.assert(requestedPath.charAt(0) == "/");
      retrieveFromCache(loadPromise, requestedPath);

    } else {
      if (requestedPath.charAt(0) == "/") {
        // Need to find the module in node.libraryPaths
        findPath(requestedPath, node.libraryPaths, function (fullPath) {
          if (fullPath) {
            retrieveFromCache(loadPromise, fullPath, parent);
          } else {
            loadPromise.emitError(new Error("Cannot find module '" + requestedPath + "'"));
          }
        });

      } else {
        // Relative file load
        var fullPath = node.path.join(node.path.dirname(parent.filename),
            requestedPath);
        retrieveFromCache(loadPromise, fullPath, parent);
      }
    }

    return loadPromise;
  };
}());

node.Module.prototype.load = function (loadPromise) {
  if (this.loaded) {
    loadPromise.emitError(new Error("Module '" + self.filename + "' is already loaded."));
    return;
  }
  node.assert(!node.loadPromise);
  this.loadPromise = loadPromise;

  if (this.filename.match(/\.node$/)) {
    this.loadObject(loadPromise);
  } else {
    this.loadScript(loadPromise);
  }
};

node.Module.prototype.loadObject = function (loadPromise) {
  var self = this;
  // XXX Not yet supporting loading from HTTP. would need to download the
  // file, store it to tmp then run dlopen on it. 
  node.fs.exists(self.filename, function (does_exist) {
    if (does_exist) {
      self.loaded = true;
      node.dlopen(self.filename, self.exports); // FIXME synchronus
      loadPromise.emitSuccess(self.exports);
    } else {
      loadPromise.emitError(new Error("Error reading " + self.filename));
    }
  });
};

node.Module.prototype.loadScript = function (loadPromise) {
  var self = this;
  var catPromise = node.cat(self.filename);

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

    require.async = requireAsync;

    // create wrapper function
    var wrapper = "var __wrap__ = function (__module, __filename, exports, require) { " 
                + content 
                + "\n}; __wrap__;";
    var compiled_wrapper = node.compile(wrapper, self.filename);

    compiled_wrapper.apply(self.exports, [self, self.filename, self.exports, require]);

    self.waitChildrenLoad(function () {
      self.loaded = true;
      loadPromise.emitSuccess(self.exports);
    });
  });
};

node.Module.prototype.newChild = function (path) {
  return node.loadModule(path, {}, this);
};

node.Module.prototype.waitChildrenLoad = function (callback) {
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
  node.reallyExit(code);
};

node.exit = function (code) {
  throw new Error("node.exit() has been renamed to process.exit().");
};

(function () {
  var cwd = node.cwd();

  // Make ARGV[0] and ARGV[1] into full paths.
  if (ARGV[0].charAt(0) != "/") {
    ARGV[0] = node.path.join(cwd, ARGV[0]);
  }

  if (ARGV[1].charAt(0) != "/") {
    ARGV[1] = node.path.join(cwd, ARGV[1]);
  }

  // Load the root module--the command line argument.
  node.loadModule(ARGV[1], process);
}());
