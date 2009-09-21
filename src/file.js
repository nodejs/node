node.fs.exists = function (path, callback) {
  var p = node.fs.stat(path);
  p.addCallback(function () { callback(true); });
  p.addErrback(function () { callback(false); });
};

node.fs.cat = function (path, encoding) {
  var open_promise = node.fs.open(path, node.O_RDONLY, 0666);
  var cat_promise = new node.Promise();

  encoding = encoding || "utf8";

  open_promise.addErrback(function () {
    cat_promise.emitError(new Error("Could not open " + path));
  });
  open_promise.addCallback(function (fd) {
    var content = "";
    var pos = 0;

    function readChunk () {
      var read_promise = node.fs.read(fd, 16*1024, pos, encoding);

      read_promise.addErrback(function () { cat_promise.emitError(); });

      read_promise.addCallback(function (chunk, bytes_read) {
        if (chunk) {
          if (chunk.constructor == String)
            content += chunk;
          else
            content = content.concat(chunk);

          pos += bytes_read;
          readChunk();
        } else {
          cat_promise.emitSuccess(content);
          node.fs.close(fd);
        }
      });
    }
    readChunk();
  });
  return cat_promise;
};

node.fs.Stats.prototype._checkModeProperty = function (property) {
  return ((this.mode & property) == property);
};

node.fs.Stats.prototype.isDirectory = function () {
  return this._checkModeProperty(node.S_IFDIR);
};

node.fs.Stats.prototype.isFile = function () {
  return this._checkModeProperty(node.S_IFREG);
};

node.fs.Stats.prototype.isBlockDevice = function () {
  return this._checkModeProperty(node.S_IFBLK);
};

node.fs.Stats.prototype.isCharacterDevice = function () {
  return this._checkModeProperty(node.S_IFCHR);
};

node.fs.Stats.prototype.isSymbolicLink = function () {
  return this._checkModeProperty(node.S_IFLNK);
};

node.fs.Stats.prototype.isFIFO = function () {
  return this._checkModeProperty(node.S_IFIFO);
};

node.fs.Stats.prototype.isSocket = function () {
  return this._checkModeProperty(node.S_IFSOCK);
};
