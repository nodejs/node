File.rename = function (file1, file2, callback) {
  this._addAction("rename", [file1, file2], callback);
};

File.stat = function (path, callback) {
  this._addAction("stat", [path], callback);
};

File.exists = function (path, callback) {
  this._addAction("stat", [path], function (status) {
      callback(status == 0);
  });
}

File.cat = function (path, encoding, callback) {
  var content = "";
  var file = new File();
  file.encoding = "utf8";
  var pos = 0;
  var chunkSize = 10*1024;

  function readChunk () {
    file.read(chunkSize, pos, function (status, chunk) {
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

  file.open(path, "r", function (status) {
    if (status == 0) 
      readChunk();
    else
      callback (status);
  });
}

File.prototype.puts = function (data, callback) {
  this.write(data + "\n", -1, callback);
};

File.prototype.print = function (data, callback) {
  this.write(data, -1, callback);
};

File.prototype.open = function (path, mode, callback) {
  this._addAction("open", [path, mode], callback);
};

File.prototype.close = function (callback) {
  this._addAction("close", [], callback);
};

File.prototype.write = function (buf, pos, callback) {
  this._addAction("write", [buf, pos], callback);
};

File.prototype.read = function (length, pos, callback) {
  this._addAction("read", [length, pos], callback);
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
// reach the head of the queue they will be executed. C++ calles the
// method _pollActions each time it becomes idle. If there is no action
// currently being executed then _pollActions will not be called. Thus when
// actions are added to an empty _actionQueue, they should be immediately
// executed.
//
// When an action has completed, the C++ side is going to look at the first
// element of _actionQueue in order to get a handle on the callback
// function. Only after that completion callback has been made can the
// action be shifted out of the queue.
// 
// See File::CallTopCallback() in file.cc to see the other side of the
// binding.
File._addAction = File.prototype._addAction = function (method, args, callback) {
  this._actionQueue.push({ method: method 
                         , callback: callback
                         , args: args
                         });
  if (this._actionQueue.length == 1) this._act();
}

File._act = File.prototype._act = function () {
  var action = this._actionQueue[0];
  if (action)
    // TODO FIXME what if the action throws an error?
    this["_ffi_" + action.method].apply(this, action.args);
};

// called from C++ after each action finishes
// (i.e. when it returns from the thread pool)
File._pollActions = File.prototype._pollActions = function () {
  this._actionQueue.shift();
  this._act();
};

var stdout = new File();
stdout.fd = File.STDOUT_FILENO;

var stderr = new File();
stderr.fd = File.STDERR_FILENO;

var stdin = new File();
stdin.fd = File.STDIN_FILENO;

this.puts = function (data, callback) {
  stdout.puts(data, callback);
}
