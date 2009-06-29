node.fs.exists = function (path, callback) {
  var p = node.fs.stat(path);
  p.addCallback(function () { callback(true); });
  p.addErrback(function () { callback(false); });
}

node.fs.cat = function (path, encoding, callback) {
  var open_promise = node.fs.open(path, node.O_RDONLY, 0666);
  var cat_promise = new node.Promise();

  encoding = (encoding === "raw" ? node.RAW : node.UTF8);

  open_promise.addErrback(function () { cat_promise.emitError(); });
  open_promise.addCallback(function (fd) {
    var content = (encoding == node.UTF8 ? "" : []);
    var pos = 0;

    function readChunk () {
      var read_promise = node.fs.read(fd, 16*1024, pos, encoding);

      read_promise.addErrback(function () { cat_promise.emitError(); });

      read_promise.addCallback(function (chunk) {
        if (chunk) {
          if (chunk.constructor == String)
            content += chunk;
          else
            content = content.concat(chunk);

          pos += chunk.length;
          readChunk();
        } else {
          cat_promise.emitSuccess([content]);
          node.fs.close(fd);
        }
      });
    }
    readChunk();
  });
  return cat_promise;
};

node.fs.File = function (options) {
  var self = this;
  self.__proto__ = new node.EventEmitter();

  options = options || {};

  if (options.encoding === "utf8") {
    self.encoding = node.UTF8;
  } else {
    self.encoding = node.RAW;
  }
  //node.debug("encoding: opts=" + options.encoding + " self=" + self.encoding);
  self.fd = options.fd || null;

  var actionQueue = [];
  
  // Adds a method to the queue. 
  function createAction (method, args) {
    var promise = new node.Promise();
   
    promise.method = method;
    promise.args = args;

    //node.debug("add action " + method + " " + JSON.stringify(args));

    actionQueue.push(promise);

    // If the queue was empty, immediately call the method.
    if (actionQueue.length == 1) act();

    return promise;
  }

  function act () {
    var promise = actionQueue[0]; // peek at the head of the queue
    if (promise) {
      //node.debug("internal apply " + JSON.stringify(promise.args));
      internal_methods[promise.method].apply(self, promise.args);
    }
  }

  // called after each action finishes (when it returns from the thread pool)
  function success () {
    //node.debug("success called");

    var promise = actionQueue[0];

    if (!promise) throw "actionQueue empty when it shouldn't be.";

    promise.emitSuccess(arguments);

    actionQueue.shift();
    act();
  }

  function error () {
    var promise = actionQueue[0];

    if (!promise) throw "actionQueue empty when it shouldn't be.";

    promise.emitError(arguments);
    self.emitError(arguments);
  }

  var internal_methods = {
    open: function (path, mode) {
      var flags;
      switch (mode) {
        case "r":
          flags = node.O_RDONLY;
          break;
        case "r+":
          flags = node.O_RDWR;
          break;
        case "w":
          flags = node.O_CREAT | node.O_TRUNC | node.O_WRONLY;
          break;
        case "w+":
          flags = node.O_CREAT | node.O_TRUNC | node.O_RDWR;
          break;
        case "a":
          flags = node.O_APPEND | node.O_CREAT | node.O_WRONLY; 
          break;
        case "a+":
          flags = node.O_APPEND | node.O_CREAT | node.O_RDWR; 
          break;
        default:
          throw "Unknown mode";
      }
      // fix the mode here
      var promise = node.fs.open(path, flags, 0666);
      
      promise.addCallback(function (fd) {
        self.fd = fd;
        success(fd);
      });

      promise.addErrback(error);
    },

    close: function ( ) {
      var promise = node.fs.close(self.fd);

      promise.addCallback(function () {
        self.fd = null;
        success();
      });

      promise.addErrback(error);
    }, 

    read: function (length, position) {
      //node.debug("encoding: " + self.encoding);
      var promise = node.fs.read(self.fd, length, position, self.encoding);
      promise.addCallback(success);  
      promise.addErrback(error);  
    },

    write: function (data, position) {
      //node.debug("internal write");
      var promise = node.fs.write(self.fd, data, position);
      promise.addCallback(success);  
      promise.addErrback(error);  
    }
  };

  self.open = function (path, mode) {
    return createAction("open", [path, mode]);
  };
   
  self.close = function () {
    return createAction("close", []);
  };

  self.read = function (length, pos) {
    return createAction("read", [length, pos]);
  };

  self.write = function (buf, pos) {
    //node.debug("external write");
    return createAction("write", [buf, pos]);
  };

  self.print = function (data) {
    return self.write(data, null);
  };

  self.puts = function (data) {
    return self.write(data + "\n", null);
  };
};

stdout = new node.fs.File({ fd: node.STDOUT_FILENO });
stderr = new node.fs.File({ fd: node.STDERR_FILENO });
stdin  = new node.fs.File({ fd: node.STDIN_FILENO  });

puts  = stdout.puts;
print = stdout.print;
p = function (data) {
  return puts(JSON.stringify(data));
}

