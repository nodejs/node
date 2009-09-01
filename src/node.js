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

node.loadingModules = [];

function require_async (url) {
  var currentModule = node.loadingModules[0];
  return currentModule.newChild(url, {});
}

function require (url) {
  return require_async(url).wait();
}

function include_async (url) {
  var currentModule = node.loadingModules[0];
  return currentModule.newChild(url, currentModule.target);
}

function include (url) {
  include_async(url).wait();
}

node.Module = function (o) {
  this.parent = o.parent;
  this.target = o.target || {};

  if (!o.path) throw "path argument required";

  if (o.path.charAt(0) == "/") {
    throw "Absolute module paths are not yet supported by Node";
  }

  if (o.path.match(/:\/\//)) {
    this.filename = o.path;
  } else {
    var dir = o.base_directory || ".";
    this.filename = node.path.join(dir, o.path);
  }

  this.loaded = false;
  this.loadPromise = null;
  this.exited = false;
  this.children = [];
};

node.Module.prototype.load = function (callback) {
  if (this.filename.match(/\.node$/)) {
    return this.loadObject(callback);
  } else {
    return this.loadScript(callback);
  }
};

node.Module.prototype.loadObject = function (callback) {
  var self = this;
  var loadPromise = new node.Promise();
  self.loadPromise = loadPromise;
  // XXX Not yet supporting loading from HTTP. would need to download the
  // file, store it to tmp then run dlopen on it. 
  node.fs.exists(self.filename, function (does_exist) {
    if (does_exist) {
      self.loaded = true;
      node.dlopen(self.filename, self.target); // FIXME synchronus
      loadPromise.emitSuccess([self.target]);
    } else {
      node.stdio.writeError("Error reading " + self.filename + "\n");
      loadPromise.emitError();
    }
  });
  return loadPromise;
};

node.Module.prototype.loadScript = function (callback) {
  var self = this;
  if (self.loaded) {
    throw "Module '" + self.filename + "' is already loaded.";
  }

  var loadPromise = new node.Promise();
  node.assert(self.loadPromise === null);
  self.loadPromise = loadPromise;

  var cat_promise = node.cat(self.filename, "utf8");

  cat_promise.addErrback(function () {
    node.stdio.writeError("Error reading " + self.filename + "\n");
    loadPromise.emitError();
  });

  cat_promise.addCallback(function (content) {
    // remove shebang
    content = content.replace(/^\#\!.*/, '');

    // create wrapper function
    var wrapper = "function (__filename) { "+
                  "  var onLoad; "+
                  "  var onExit; "+
                  "  var exports = this; "+
                  content+
                  "\n"+
                  "  this.__onLoad = onLoad;\n"+
                  "  this.__onExit = onExit;\n"+
                  "};\n";
    var compiled_wrapper = node.compile(wrapper, self.filename);

    node.loadingModules.unshift(self);
    compiled_wrapper.apply(self.target, [self.filename]);
    node.loadingModules.shift();

    self.onLoad = self.target.__onLoad;
    self.onExit = self.target.__onExit;
    if (self.onLoad || self.onExit) {
      node.stdio.writeError( "(node) onLoad is depreciated it will be "
                           + "removed in the future. Don't want it to "
                           + "leave? Discuss on mailing list.\n"
                           );
    }

    self.waitChildrenLoad(function () {
      if (self.onLoad) {
        self.onLoad();
      }
      self.loaded = true;
      loadPromise.emitSuccess([self.target]);
    });
  });
};

node.Module.prototype.newChild = function (path, target) {
  var child = new node.Module({
    target: target,
    path: path,
    base_directory: node.path.dirname(this.filename),
    parent: this
  });
  this.children.push(child);
  child.load();
  return child.loadPromise;
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

node.Module.prototype.exitChildren = function (callback) {
  var children = this.children;
  if (children.length == 0 && callback) callback();
  var nexited = 0;
  for (var i = 0; i < children.length; i++) {
    children[i].exit(function () {
      nexited += 1;
      if (nexited == children.length && callback) callback();
    });
  }
};

node.Module.prototype.exit = function (callback) {
  var self = this;

  if (self.exited) {
    throw "Module '" + self.filename + "' is already exited.";
  }

  this.exitChildren(function () {
    if (self.onExit) self.onExit();
    self.exited = true;
    if (callback) callback();
  });
};

(function () {
  // Load the root module--the command line argument.
  var root_module = new node.Module({
    path: node.path.filename(ARGV[1]),
    base_directory: node.path.dirname(ARGV[1]),
    target: this
  });
  root_module.load();

  node.exit = function (code) {
    root_module.exit(function () {
      process.emit("exit");
      node.reallyExit(code);
    });
  };
}());
