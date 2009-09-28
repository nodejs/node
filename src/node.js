node.tcp.createServer = function (on_connection, options) {
  var server = new node.tcp.Server();
  server.addListener("connection", on_connection);
  //server.setOptions(options);
  return server;
};

node.createProcess = function () {
  throw "node.createProcess() has been changed to node.createChildProcess() update your code";
};

node.createChildProcess = function (command) {
  var child = new node.ChildProcess();
  child.spawn(command);
  return child;
};

node.exec = function () {
  throw new Error("node.exec() has moved. Use include('/utils.js') to bring it back.");
}

node.http.createServer = function () {
  throw new Error("node.http.createServer() has moved. Use require('/http.js') to access it.");
}

node.http.createClient = function () {
  throw new Error("node.http.createClient() has moved. Use require('/http.js') to access it.");
}

node.tcp.createConnection = function (port, host) {
  var connection = new node.tcp.Connection();
  connection.connect(port, host);
  return connection;
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
  delete timer;
}

clearInterval = clearTimeout;

// Module

node.libraryPaths = [ node.path.join(ENV["HOME"], ".node_libraries")
                    , node.path.join(node.installPrefix, "lib/node_libraries")
                    , "/" 
                    ];

if (ENV["NODE_LIBRARY_PATHS"]) {
  node.libraryPaths =
    ENV["NODE_LIBRARY_PATHS"].split(":").concat(node.libraryPaths);
}

node.loadingModules = [];

function require_async (url) {
  var currentModule = node.loadingModules[0];
  return currentModule.newChild(url, {});
}

function require (url) {
  return require_async(url).wait();
}

function include_async (url) {
  var promise = require_async(url)
  promise.addCallback(function (t) {
    // copy properties into global namespace.
    for (var prop in t) {
      if (t.hasOwnProperty(prop)) process[prop] = t[prop];
    }
  });
  return promise;
}

function include (url) {
  include_async(url).wait();
}

node.Module = function (filename, parent) {
  node.assert(filename.charAt(0) == "/");
  this.filename = filename;
  this.target = {};
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
        loadPromise.emitSuccess(module.target);
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

  node.loadModule = function (requestedPath, target, parent) {
    var loadPromise = new node.Promise();

    // On success copy the loaded properties into the target
    loadPromise.addCallback(function (t) {
      for (var prop in t) {
        if (t.hasOwnProperty(prop)) target[prop] = t[prop];
      }
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
      node.dlopen(self.filename, self.target); // FIXME synchronus
      loadPromise.emitSuccess(self.target);
    } else {
      loadPromise.emitError(new Error("Error reading " + self.filename));
      node.exit(1);
    }
  });
};

node.Module.prototype.loadScript = function (loadPromise) {
  var self = this;
  var catPromise = node.cat(self.filename);

  catPromise.addErrback(function () {
    loadPromise.emitError(new Error("Error reading " + self.filename));
    node.exit(1);
  });

  catPromise.addCallback(function (content) {
    // remove shebang
    content = content.replace(/^\#\!.*/, '');

    // create wrapper function
    var wrapper = "function (__filename, exports) { " + content + "\n};";
    var compiled_wrapper = node.compile(wrapper, self.filename);

    node.loadingModules.unshift(self);
    compiled_wrapper.apply(self.target, [self.filename, self.target]);
    node.loadingModules.shift();

    self.waitChildrenLoad(function () {
      self.loaded = true;
      loadPromise.emitSuccess(self.target);
    });
  });
};

node.Module.prototype.newChild = function (path, target) {
  return node.loadModule(path, target, this);
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

  node.exit = function (code) {
    process.emit("exit");
    node.reallyExit(code);
  };
}());
