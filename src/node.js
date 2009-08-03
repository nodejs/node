node.tcp.createServer = function (on_connection, options) {
  var server = new node.tcp.Server();
  server.addListener("connection", on_connection);
  //server.setOptions(options);
  return server;
};

node.createProcess = function (command) {
  var process = new node.Process();
  process.spawn(command);
  return process;
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
  this.exited = false;
  this.children = [];
};

node.Module.prototype.load = function (callback) {
  var self = this;
  if (self.loaded) {
    throw "Module '" + self.filename + "' is already loaded.";
  }

  var promise = node.cat(self.filename, "utf8");
  
  promise.addErrback(function () {
    stderr.puts("Error reading " + self.filename);
    node.exit(1);
  });

  promise.addCallback(function (content) {
    self.target.__require = function (path) { return self.newChild(path, {}); };
    self.target.__include = function (path) { self.newChild(path, self.target); };

    // remove shebang
    content = content.replace(/^\#\!.*/, '');

    // create wrapper function
    var wrapper = "function (__filename) { "+
                  "  var onLoad; "+
                  "  var onExit; "+
                  "  var exports = this; "+
                  "  var require = this.__require; "+
                  "  var include = this.__include; "+
                  content+
                  "\n"+
                  "  this.__onLoad = onLoad;\n"+
                  "  this.__onExit = onExit;\n"+
                  "};\n";
    var compiled_wrapper = node.compile(wrapper, self.filename);
    compiled_wrapper.apply(self.target, [self.filename]);
    self.onLoad = self.target.__onLoad;
    self.onExit = self.target.__onExit;

    self.loadChildren(function () {
      if (self.onLoad) self.onLoad();
      self.loaded = true;
      if (callback) callback();
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
  return target;
};

node.Module.prototype.loadChildren = function (callback) {
  var children = this.children;
  if (children.length == 0 && callback) callback();
  var nloaded = 0;
  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    child.load(function () {
      nloaded += 1;
      if (nloaded == children.length && callback) callback(); 
    });
  }
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
      node.reallyExit(code);
    });
  };
}());
