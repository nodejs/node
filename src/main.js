// module search paths
node.includes = ["."];

// This is useful for dealing with raw encodings.
Array.prototype.encodeUtf8 = function () {
  return String.fromCharCode.apply(String, this);
}

node.path = new function () {
  this.join = function () {
    var joined = "";
    for (var i = 0; i < arguments.length; i++) {
      var part = arguments[i].toString();
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
};

// Timers

function setTimeout (callback, delay) {
  var timer = new Timer(callback, delay, 0); 
  timer.start();
  return timer;
};

function setInterval (callback, delay) {
  var timer = new Timer(callback, delay, delay); 
  timer.start();
  return timer;
};

function clearTimeout (timer) {
  timer.stop();
  delete timer;
};
clearInterval = clearTimeout;

// Modules

(function () {
  function findScript(base_directory, name, callback) {
    // in the future this function will be more complicated
    if (name.charAt(0) == "/")
      throw "absolute module paths are not yet supported.";

    var filename = node.path.join(base_directory, name) + ".js";

    File.exists(filename, function (status) {
      callback(status ? filename : null);
    });
  }

  // Constructor for submodule.
  // "name" is like a path but without .js. e.g. "database/mysql"
  // "target" is an object into which the submodule will be loaded.
  function Sub (name, target) {
    this.name = name;
    this.target = target;

    this.load = function (base_directory, callback) {
      //node.debug("sub.load from <" + base_directory + ">  " + this.toString());
      findScript(base_directory, name, function (filename) {
        if (filename === null) {
          stderr.puts("Cannot find a script matching: " + name);
          exit(1);
        }
        loadScript(filename, target, callback);
      });
    };

    this.toString = function () {
      return "[sub name=" + name + " target=" + target.toString() + "]";
    }
  }

  function Scaffold (source, filename, module) {
    // wrap the source in a strange function 
    var source = "function (__filename) {" 
               + "  var onLoad;"
               + "  var exports = this;"
               + "  var require = this.__require;"
               + "  var include = this.__include;"
               +    source 
               + "  this.__onLoad = onLoad;"
               + "};"
               ;
    // returns the function       
    var compiled = node.compile(source, filename);

    if (module.__onLoad) {
      //node.debug("<"+ filename+"> has onload! this is bad");
    }

    module.__subs = [];
    module.__require = function (name) {
      var target = {};
      module.__subs.push(new Sub(name, target));
      return target;
    }
    module.__include = function (name) {
      module.__subs.push(new Sub(name, module));
    }
    // execute the script of interest
    compiled.apply(module, [filename]);

    // The module still needs to have its submodules loaded.
    this.filename = filename;
    this.module  = module;
    this.subs    = module.__subs;
    this.onLoad = module.__onLoad;

    // remove these references so they don't get exported.
    delete module.__subs;
    delete module.__onLoad;
    delete module.__require;
    delete module.__include;
  }

  function loadScript (filename, target, callback) {
    File.cat(filename, function (status, content) {
      if (status != 0) {
        stderr.puts("Error reading " + filename + ": " + File.strerror(status));
        exit(1);
      }
      
      var scaffold = new Scaffold(content, filename, target);

      //node.debug("after scaffold <" + filename + ">");

      function finish() {
        //node.debug("finish 1 load <" + filename + ">");
        if (scaffold.onLoad instanceof Function) {
          //node.debug("calling onLoad for <" + filename + ">"); 
          scaffold.onLoad(); 
        }
        //node.debug("finish 2 load <" + filename + ">");

        if (callback instanceof Function)
          callback();
      }

      // Each time require() or include() was called inside the script 
      // a key/value was added to scaffold.__subs.
      // Now we loop though each one and recursively load each.
      if (scaffold.subs.length == 0) {
        finish(); 
      } else {
        var ncomplete = 0;
        for (var i = 0; i < scaffold.subs.length; i++) {
          var sub = scaffold.subs[i];
          sub.load(node.path.dirname(filename), function () {
            ncomplete += 1;
            //node.debug("<" + filename + "> ncomplete = " + ncomplete.toString() + " scaffold.subs.length = " + scaffold.subs.length.toString());
            if (ncomplete === scaffold.subs.length)
              finish();
          });
        }
      }
    });
  }

  loadScript(ARGV[1], this);
})();

