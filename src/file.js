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

File.prototype.puts = function (data, callback) {
  this.write(data + "\n", callback);
};

File.prototype.open = function (path, mode, callback) {
  this._addAction("open", [path, mode], callback);
};

File.prototype.close = function (callback) {
  this._addAction("close", [], callback);
};

File.prototype.write = function (buf, callback) {
  this._addAction("write", [buf], callback);
};

File.prototype.read = function (length, callback) {
  this._addAction("read", [length], callback);
};

File.prototype._addAction = function (method, args, callback) {
  this._actionQueue.push({ method: method 
                         , callback: callback
                         , args: args
                         });
  if (this._actionQueue.length == 1) this._act();
}

File.prototype._act = function () {
  var action = this._actionQueue[0];
  if (action)
    this["_ffi_" + action.method].apply(this, action.args);
};

// called from C++ after each action finishes
// (i.e. when it returns from the thread pool)
File.prototype._pollActions = function () {
  this._actionQueue.shift();
  this._act();
};

var stdout = new File();
stdout.fd = File.STDOUT_FILENO;

var stderr = new File();
stderr.fd = File.STDERR_FILENO;



Array.prototype.encodeUtf8 = function () {
  return String.fromCharCode.apply(String, this);
}
