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
    var parts = path.split("/");
    return parts.slice(0, parts.length-1);
  };
};

// Namespace for module loading functionality
(function () {
  function findScript(base_directory, name, callback) {
    // in the future this function will be more complicated
    if (name.charAt(0) == "/")
      throw "absolute module paths are not yet supported.";

    var filename = node.path.join(base_directory, name) + ".js";
    File.exists(filename, function (status) {
      if (status) 
        callback(filename);
      else
        callback(null);
    });
  }

  // Constructor for submodule.
  // "name" is like a path but without .js. e.g. "database/mysql"
  // "target" is an object into which the submodule will be loaded.
  function Sub (name, target) {
    this.name = name;
    this.target = target;

    //puts("new Sub. name = " + name);

    this.load = function (base_directory, callback) {
      findScript(base_directory, name, function (filename) {
        if (filename === null) {
          stderr.puts("Cannot find a script matching: " + name);
          process.exit(1);
        }
        //puts("load subscript: " + filename);
        //puts("    target =  " + target);
        loadScript(filename, target, callback);
      });
    };

    this.toString = function () {
      return "[sub name=" + name + " target=" + target.toString() + "]";
    }
  }

  function Scaffold (source, filename, module) {
    // wrap the source in a strange function 
    var source = "function (__filename) {\n" 
               + "  var on_load;\n"
               + "  var exports = this;\n"
               + "  var require = this.__require;\n"
               + "  var include = this.__include;\n"
               +    source + "\n"
               + "  this.__on_load = on_load;\n"
               + "};"
               ;
    // returns the function       
    var compiled = node.compile(source, filename);

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
    this.module = module;
    this.subs = module.__subs;
    this.on_load = module.__on_load;

    /*
    puts("new Scaffold.");
    for(var i = 0; i < this.subs.length; i++) {
      puts("-  subs[" + i.toString() + "] " + this.subs[i].toString());
    }
    */

    // remove these references so they don't get exported.
    delete module.__subs;
    delete module.__on_load;
    delete module.__require;
    delete module.__include;
  }

  function loadScript (filename, target, callback) {
    File.cat(filename, function (status, content) {
      if (status != 0) {
        stderr.puts("Error reading " + filename + ": " + File.strerror(status));
        process.exit(1);
      }

      //puts("loadScript: " + filename);
      
      var scaffold = new Scaffold(content, filename, target);

      function finish() {
        if (scaffold.on_load instanceof Function)
          scaffold.on_load(); 
        else
          ; //puts("no on_load for " + filename + " ..  scaffold.on_load = " + scaffold.on_load);

        if (callback instanceof Function)
          callback();
        else
          ; //puts("no loadScript callback for " + filename);
      }

      // Each time require() or include() was called inside the script 
      // a key/value was added to scaffold.__subs.
      // Now we loop though each one and recursively load each.
      if (scaffold.subs.length == 0) {
        finish(); 
      } else {
        while (scaffold.subs.length > 0) {
          var sub = scaffold.subs.shift();
          sub.load(node.path.dirname(filename), function () {
            //puts("finish sub load: " + sub.name);
            if(scaffold.subs.length != 0) return;
            finish();
          });
        }
      }
    });
  }

  loadScript(ARGV[1], this);
})();

