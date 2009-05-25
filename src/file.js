node.fs.exists = function (path, callback) {
  node.fs.stat(path, function (status) {
    callback(status == 0);
  });
}

node.fs.cat = function (path, encoding, callback) {
  var file = new node.fs.File();
  file.encoding = encoding;

  file.onError = function (method, errno, msg) {
    callback(-1);
  };

  var content = (file.encoding == "raw" ? "" : []);
  var pos = 0;
  var chunkSize = 10*1024;

  function readChunk () {
    file.read(chunkSize, pos, function (chunk) {
      if (chunk) {
        if (chunk.constructor == String)
          content += chunk;
        else
          content = content.concat(chunk);

        pos += chunk.length;
        readChunk();
      } else {
        callback(0, content);
        file.close();
      }
    });
  }

  file.open(path, "r", function () {
    readChunk();
  });
}

node.fs.File.prototype.open = function (path, mode, callback) {
  this._addAction("open", [path, mode], callback);
};

node.fs.File.prototype.close = function (callback) {
  this._addAction("close", [], callback);
};

node.fs.File.prototype.read = function (length, pos, callback) {
  this._addAction("read", [length, pos], callback);
};

node.fs.File.prototype.write = function (buf, pos, callback) {
  this._addAction("write", [buf, pos], callback);
};

node.fs.File.prototype.print = function (data, callback) {
  this.write(data, -1, callback);
};

node.fs.File.prototype.puts = function (data, callback) {
  this.write(data + "\n", -1, callback);
};

// Some explanation of the File binding.
//
// All file operations are blocking. To get around this they are executed
// in a thread pool in C++ (libeio). 
//
// The ordering of method calls to a file should be preserved, so they are
// only executed one at a time. A queue, called _actionQueue is employed. 
//
// The constructor File() is implemented in C++. It initlizes 
// the member _actionQueue = []
//
// Any of the methods called on a file are put into this queue. When they
// reach the head of the queue they will be executed. C++ calls the
// method _pollActions each time it becomes idle. If there is no action
// currently being executed then _pollActions will not be called. When
// actions are added to an empty _actionQueue, they will be immediately
// executed.
//
// When an action has completed, the C++ side is looks at the first
// element of _actionQueue in order to get a handle on the callback
// function. Only after that completion callback has been made can the
// action be shifted out of the queue.
// 
// See File::CallTopCallback() in file.cc for the other side of the binding.


node.fs.File.prototype._addAction = function (method, args, callback) {
  // This adds a method to the queue. 
  var action = { method: method 
               , callback: callback
               , args: args
               };
  this._actionQueue.push(action);

  // If the queue was empty, immediately call the method.
  if (this._actionQueue.length == 1) this._act();
};

node.fs.File.prototype._act = function () {
  // peek at the head of the queue
  var action = this._actionQueue[0];
  if (action) {
    // execute the c++ version of the method. the c++ version 
    // is gotten by appending "_ffi_" to the method name.
    this["_ffi_" + action.method].apply(this, action.args);
  }
};

// called from C++ after each action finishes
// (i.e. when it returns from the thread pool)
node.fs.File.prototype._pollActions = function () {
  var action = this._actionQueue[0];

  var errno = arguments[0]; 

  if (errno < 0) {
    if (this.onError)
      this.onError(action.method, errno, node.fs.strerror(errno));
    this._actionQueue = []; // empty the queue.
    return;
  }

  var rest = [];
  for (var i = 1; i < arguments.length; i++)
    rest.push(arguments[i]);

  if (action.callback)
    action.callback.apply(this, rest);

  this._actionQueue.shift();

  this._act();
};

stdout = new node.fs.File();
stdout.fd = node.fs.File.STDOUT_FILENO;

stderr = new node.fs.File();
stderr.fd = node.fs.File.STDERR_FILENO;

stdin = new node.fs.File();
stdin.fd = node.fs.File.STDIN_FILENO;

puts = function (data, callback) {
  stdout.puts(data, callback);
}

p = function (data, callback) {
  puts(JSON.stringify(data), callback);
}
