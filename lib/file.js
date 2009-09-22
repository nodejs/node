/*jslint onevar: true, undef: true, eqeqeq: true, plusplus: true, regexp: true, newcap: true, immed: true */
/*globals exports, node, __filename */

exports.debugLevel = 0; // Increase to get more verbose debug output

function debugMessage (msg) {
  if (exports.debugLevel > 0) {
    node.error(__filename + ": " + msg.toString());
  }
}

function debugObject (obj) {
  if (exports.debugLevel > 0) {
    node.error(__filename + ": " + JSON.stringify(obj));
  }
}

exports.read = node.fs.cat;

exports.write = function (filename, data, encoding) {
  var promise = new node.Promise();

  encoding = encoding || "utf8"; // default to utf8

  node.fs.open(filename, node.O_WRONLY | node.O_TRUNC | node.O_CREAT, 0666)
    .addCallback(function (fd) {
      function doWrite (_data) {
        node.fs.write(fd, _data, 0, encoding)
          .addErrback(function () {
            node.fs.close(fd);
          })
          .addCallback(function (written) {
            if (written === _data.length) {
              node.fs.close(fd);
            } else {
              doWrite(_data.slice(written));
            }
          });
      }
      doWrite(data);
    })
    .addErrback(function () {
      promise.emitError();
    });

  return promise;
};

exports.File = function (filename, mode, options) {
  var self = this;

  options = options || {};
  self.encoding = options.encoding || "utf8";

  self.filename = filename;

  self.actionQueue = [];
  self.currentAction = null;

  switch (mode) {
    case "r":
      self.flags = node.O_RDONLY;
      break;

    case "r+":
      self.flags = node.O_RDWR;
      break;

    case "w":
      self.flags = node.O_CREAT | node.O_TRUNC | node.O_WRONLY;
      break;

    case "w+":
      self.flags = node.O_CREAT | node.O_TRUNC | node.O_RDWR;
      break;

    case "a":
      self.flags = node.O_APPEND | node.O_CREAT | node.O_WRONLY; 
      break;

    case "a+":
      self.flags = node.O_APPEND | node.O_CREAT | node.O_RDWR; 
      break;

    default:
      throw new Error("Unknown mode");
  }

  self.open(self.filename, self.flags, 0666).addCallback(function (fd) {
    debugMessage(self.filename + " opened. fd = " + fd);
    self.fd = fd;
  }).addErrback(function () {
    self.emit("error", ["open"]);
  });
};

var proto = exports.File.prototype;

proto._maybeDispatch = function () {
  var self, args, method, promise, userPromise;
  
  self = this;

  if (self.currentAction) { return; }
  self.currentAction = self.actionQueue.shift();
  if (!self.currentAction) { return; }

  debugObject(self.currentAction);

  args = self.currentAction.args || [];

  if (self.currentAction.method !== "open") {
    args.unshift(self.fd);
  }

  method = self.currentAction.method;

  if (!args[3] && (method === "read" || method === "write")) {
    args[3] = self.encoding;
  }

  promise = node.fs[method].apply(self, args);

  userPromise = self.currentAction.promise;

  promise.addCallback(function () {
    node.assert(self.currentAction.promise === userPromise);
    userPromise.emitSuccess.apply(userPromise, arguments);
    self.currentAction = null;
    self._maybeDispatch();
  }).addErrback(function () {
    debugMessage("Error in method " + method);
    node.assert(self.currentAction.promise === userPromise);
    userPromise.emitError.apply(userPromise, arguments);
    self.currentAction = null;
    self._maybeDispatch();
  });
};

proto._queueAction = function (method, args) {
  var userPromise = new node.Promise();
  this.actionQueue.push({ method: method, args: args, promise: userPromise });
  this._maybeDispatch();
  return userPromise;
};

// FIXME the following can probably be DRY'd up with some fancy getter
// stuff.

proto.open = function (filename, flags, mode) {
  return this._queueAction("open", [filename, flags, mode]);
};

proto.write = function (data, pos, encoding) {
  return this._queueAction("write", [data, pos, encoding]);
};

proto.read = function (length, pos, encoding) {
  return this._queueAction("read", [length, pos, encoding]);
};

proto.close = function () {
  return this._queueAction("close");
};
