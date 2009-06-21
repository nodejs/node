// Timers

function setTimeout (callback, delay) {
  var timer = new node.Timer(callback, delay, 0); 
  timer.start();
  return timer;
}

function setInterval (callback, delay) {
  var timer = new node.Timer(callback, delay, delay); 
  timer.start();
  return timer;
}

function clearTimeout (timer) {
  timer.stop();
  delete timer;
}

clearInterval = clearTimeout;

// This is useful for dealing with raw encodings.
Array.prototype.encodeUtf8 = function () {
  return String.fromCharCode.apply(String, this);
};

node.path = new function () {
  this.join = function () {
    var joined = "";
    for (var i = 0; i < arguments.length; i++) {
      var part = arguments[i].toString();

      if (part === ".") continue;
      while (/^\.\//.exec(part)) part = part.replace(/^\.\//, "");

      if (i === 0) {
        part = part.replace(/\/*$/, "/");
      } else if (i === arguments.length - 1) {
        part = part.replace(/^\/*/, "");
      } else {
        part = part.replace(/^\/*/, "")
               .replace(/\/*$/, "/");
      }
      joined += part;
    }
    return joined;
  };

  this.dirname = function (path) {
    if (path.charAt(0) !== "/") 
      path = "./" + path;
    var parts = path.split("/");
    return parts.slice(0, parts.length-1).join("/");
  };

  this.filename = function (path) {
    if (path.charAt(0) !== "/") 
      path = "./" + path;
    var parts = path.split("/");
    return parts[parts.length-1];
  };
};

node.cat = function(url_or_path, encoding, callback) {
  var uri = node.http.parseUri(url_or_path)
  if (uri.protocol) {
    node.http.cat(url_or_path, encoding, callback)
  } else {
    node.fs.cat(url_or_path, encoding, callback)
  }
}

// Module

node.Module = function (o) {
  this.parent = o.parent;
  this.target = o.target || {};

  if (!o.path)   throw "path argument required";
  if (o.path.charAt(0) == "/")
    throw "Absolute module paths are not yet supported in Node";

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
  if (self.loaded) 
    throw "Module '" + self.filename + "' is already loaded.";

  node.cat(self.filename, "utf8", function (status, content) {
    if (status != 0) {
      stderr.puts("Error reading " + self.filename);
      node.exit(1);
    }

    self.target.__require = function (path) { return self.newChild(path, {}); };
    self.target.__include = function (path) { self.newChild(path, self.target); };

    // create wrapper function
    var wrapper = "function (__filename) {\n" 
                + "  var onLoad;\n"
                + "  var onExit;\n"
                + "  var exports = this;\n"
                + "  var require = this.__require;\n"
                + "  var include = this.__include;\n"
                +    content 
                + "\n"
                + "  this.__onLoad = onLoad;\n"
                + "  this.__onExit = onExit;\n"
                + "};\n"
                ;
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

  if (self.exited) 
    throw "Module '" + self.filename + "' is already exited.";

  this.exitChildren(function () {
    if (self.onExit) {
      self.onExit();
    } 
    self.exited = true;
    if (callback) callback()
  });
};

(function () {
  // Load the root module. I.E. the command line argument.
  root_module = new node.Module({ 
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
}())
