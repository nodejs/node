var sys = require('sys');
var events = require('events');
var Buffer = require('buffer').Buffer;

var binding = process.binding('fs');
var fs = exports;

var kMinPoolSpace = 128;
var kPoolSize = 40*1024;

fs.Stats = binding.Stats;

fs.Stats.prototype._checkModeProperty = function (property) {
  return ((this.mode & process.S_IFMT) === property);
};

fs.Stats.prototype.isDirectory = function () {
  return this._checkModeProperty(process.S_IFDIR);
};

fs.Stats.prototype.isFile = function () {
  return this._checkModeProperty(process.S_IFREG);
};

fs.Stats.prototype.isBlockDevice = function () {
  return this._checkModeProperty(process.S_IFBLK);
};

fs.Stats.prototype.isCharacterDevice = function () {
  return this._checkModeProperty(process.S_IFCHR);
};

fs.Stats.prototype.isSymbolicLink = function () {
  return this._checkModeProperty(process.S_IFLNK);
};

fs.Stats.prototype.isFIFO = function () {
  return this._checkModeProperty(process.S_IFIFO);
};

fs.Stats.prototype.isSocket = function () {
  return this._checkModeProperty(process.S_IFSOCK);
};

fs.readFile = function (path, encoding_, callback) {
  var encoding = typeof(encoding_) == 'string' ? encoding_ : null;
  var callback_ = arguments[arguments.length - 1];
  var callback = (typeof(callback_) == 'function' ? callback_ : noop);
  binding.stat(path, function (err, stat) {
    if (err) { callback(err); return; }
    binding.open(path, process.O_RDONLY, 0666, function (err, fd) {
      if (err) { callback(err); return; }
      var size = stat.size;
      var buffer = new Buffer(size);
      var offset = 0;
      function doRead() {
        if (size < 1) {
          callback(null, buffer);
          return;
        }
        // position is offset or null so we can read files on unseekable mediums
        binding.read(fd, buffer, offset, size - offset, offset || null, function (err, amount) {
          if (err) {
            callback(err);
            binding.close(fd);
            return;
          }
          if (amount + offset < size) {
            offset += amount;
            doRead();
            return;
          }
          binding.close(fd);
          if (encoding) {
            try {
              var str = buffer.toString(encoding);
            } catch (err) {
              callback(err);
              return;
            }
            callback(null, str);
          } else {
            callback(null, buffer);
          }
        });
      }
      doRead();
    });
  });
};

fs.readFileSync = function (path, encoding) {
  var fd = fs.openSync(path, process.O_RDONLY, 0666);
  var stat = fs.statSync(path);
  var buffer = new Buffer(stat.size);
  var nread = 0;

  while (nread < buffer.length) {
    nread += fs.readSync(fd, buffer, nread, buffer.length - nread, null);
  }

  fs.closeSync(fd);

  if (encoding) {
    return buffer.toString(encoding);
  } else {
    return buffer;
  }
};


// Used by binding.open and friends
function stringToFlags(flag) {
  // Only mess with strings
  if (typeof flag !== 'string') {
    return flag;
  }
  switch (flag) {
    case "r": return process.O_RDONLY;
    case "r+": return process.O_RDWR;
    case "w": return process.O_CREAT | process.O_TRUNC | process.O_WRONLY;
    case "w+": return process.O_CREAT | process.O_TRUNC | process.O_RDWR;
    case "a": return process.O_APPEND | process.O_CREAT | process.O_WRONLY;
    case "a+": return process.O_APPEND | process.O_CREAT | process.O_RDWR;
    default: throw new Error("Unknown file open flag: " + flag);
  }
}

function noop () {}

// Yes, the follow could be easily DRYed up but I provide the explicit
// list to make the arguments clear.

fs.close = function (fd, callback) {
  binding.close(fd, callback || noop);
};

fs.closeSync = function (fd) {
  return binding.close(fd);
};

fs.open = function (path, flags, mode, callback) {
  if (mode === undefined) { mode = 0666; }
  binding.open(path, stringToFlags(flags), mode, callback || noop);
};

fs.openSync = function (path, flags, mode) {
  if (mode === undefined) { mode = 0666; }
  return binding.open(path, stringToFlags(flags), mode);
};

fs.read = function (fd, buffer, offset, length, position, callback) {
  if (!Buffer.isBuffer(buffer)) {
    // legacy string interface (fd, length, position, encoding, callback)
    var cb = arguments[4],
        encoding = arguments[3];
    position = arguments[2];
    length = arguments[1];
    buffer = new Buffer(length);
    offset = 0;

    callback = function(err, bytesRead) {
      if (!cb) return;

      var str = (bytesRead > 0)
        ? buffer.toString(encoding, 0, bytesRead)
        : '';

      (cb)(err, str, bytesRead);
    };
  }

  binding.read(fd, buffer, offset, length, position, callback || noop);
};

fs.readSync = function (fd, buffer, offset, length, position) {
  var legacy = false;
  if (!Buffer.isBuffer(buffer)) {
    // legacy string interface (fd, length, position, encoding, callback)
    legacy = true;
    encoding = arguments[3];
    position = arguments[2];
    length = arguments[1];
    buffer = new Buffer(length);

    offset = 0;
  }

  var r = binding.read(fd, buffer, offset, length, position);
  if (!legacy) {
    return r;
  }

  var str = (r > 0)
      ? buffer.toString(encoding, 0, r)
      : '';
  return [str, r];
};

fs.write = function (fd, buffer, offset, length, position, callback) {
  if (!Buffer.isBuffer(buffer)) {
    // legacy string interface (fd, data, position, encoding, callback)
    callback = arguments[4];
    position = arguments[2];

    buffer = new Buffer(''+arguments[1], arguments[3]);
    offset = 0;
    length = buffer.length;
  }

  binding.write(fd, buffer, offset, length, position, callback || noop);
};

fs.writeSync = function (fd, buffer, offset, length, position) {
  if (!Buffer.isBuffer(buffer)) {
    // legacy string interface (fd, data, position, encoding)
    position = arguments[2];

    buffer = new Buffer(''+arguments[1], arguments[3]);
    offset = 0;
    length = buffer.length;
  }

  return binding.write(fd, buffer, offset, length, position);
};

fs.rename = function (oldPath, newPath, callback) {
  binding.rename(oldPath, newPath, callback || noop);
};

fs.renameSync = function (oldPath, newPath) {
  return binding.rename(oldPath, newPath);
};

fs.truncate = function (fd, len, callback) {
  binding.truncate(fd, len, callback || noop);
};

fs.truncateSync = function (fd, len) {
  return binding.truncate(fd, len);
};

fs.rmdir = function (path, callback) {
  binding.rmdir(path, callback || noop);
};

fs.rmdirSync = function (path) {
  return binding.rmdir(path);
};

fs.fdatasync = function (fd, callback) {
  binding.fdatasync(fd, callback || noop);
};

fs.fdatasyncSync = function (fd) {
  return binding.fdatasync(fd);
};

fs.fsync = function (fd, callback) {
  binding.fsync(fd, callback || noop);
};

fs.fsyncSync = function (fd) {
  return binding.fsync(fd);
};

fs.mkdir = function (path, mode, callback) {
  binding.mkdir(path, mode, callback || noop);
};

fs.mkdirSync = function (path, mode) {
  return binding.mkdir(path, mode);
};

fs.sendfile = function (outFd, inFd, inOffset, length, callback) {
  binding.sendfile(outFd, inFd, inOffset, length, callback || noop);
};

fs.sendfileSync = function (outFd, inFd, inOffset, length) {
  return binding.sendfile(outFd, inFd, inOffset, length);
};

fs.readdir = function (path, callback) {
  binding.readdir(path, callback || noop);
};

fs.readdirSync = function (path) {
  return binding.readdir(path);
};

fs.fstat = function (fd, callback) {
  binding.fstat(fd, callback || noop);
};

fs.lstat = function (path, callback) {
  binding.lstat(path, callback || noop);
};

fs.stat = function (path, callback) {
  binding.stat(path, callback || noop);
};

fs.fstatSync = function (fd) {
  return binding.fstat(fd);
};

fs.lstatSync = function (path) {
  return binding.lstat(path);
};

fs.statSync = function (path) {
  return binding.stat(path);
};

fs.readlink = function (path, callback) {
  binding.readlink(path, callback || noop);
};

fs.readlinkSync = function (path) {
  return binding.readlink(path);
};

fs.symlink = function (destination, path, callback) {
  binding.symlink(destination, path, callback || noop);
};

fs.symlinkSync = function (destination, path) {
  return binding.symlink(destination, path);
};

fs.link = function (srcpath, dstpath, callback) {
  binding.link(srcpath, dstpath, callback || noop);
};

fs.linkSync = function (srcpath, dstpath) {
  return binding.link(srcpath, dstpath);
};

fs.unlink = function (path, callback) {
  binding.unlink(path, callback || noop);
};

fs.unlinkSync = function (path) {
  return binding.unlink(path);
};

fs.chmod = function (path, mode, callback) {
  binding.chmod(path, mode, callback || noop);
};

fs.chmodSync = function (path, mode) {
  return binding.chmod(path, mode);
};

fs.chown = function(path, uid, gid, callback) {
  binding.chown(path, uid, gid, callback || noop);
};

fs.chownSync = function(path, uid, gid) {
  return binding.chown(path, uid, gid);
};

function writeAll (fd, buffer, callback) {
  fs.write(fd, buffer, 0, buffer.length, null, function (writeErr, written) {
    if (writeErr) {
      fs.close(fd, function () {
        if (callback) callback(writeErr);
      });
    } else {
      if (written === buffer.length) {
        fs.close(fd, callback);
      } else {
        writeAll(fd, buffer.slice(written), callback);
      }
    }
  });
}

fs.writeFile = function (path, data, encoding_, callback) {
  var encoding = (typeof(encoding_) == 'string' ? encoding_ : 'utf8');
  var callback_ = arguments[arguments.length - 1];
  var callback = (typeof(callback_) == 'function' ? callback_ : null);
  fs.open(path, 'w', 0666, function (openErr, fd) {
    if (openErr) {
      if (callback) callback(openErr);
    } else {
      var buffer = Buffer.isBuffer(data) ? data : new Buffer(data, encoding);
      writeAll(fd, buffer, callback);
    }
  });
};

fs.writeFileSync = function (path, data, encoding) {
  encoding = encoding || "utf8"; // default to utf8
  var fd = fs.openSync(path, "w");
  var written = 0;
  while (written < data.length) {
    written += fs.writeSync(fd, data, 0, encoding);
    data = data.slice(written);
  }
  fs.closeSync(fd);
};

fs.cat = function () {
  throw new Error("fs.cat is deprecated. Please use fs.readFile instead.");
};


fs.catSync = function () {
  throw new Error("fs.catSync is deprecated. Please use fs.readFileSync instead.");
};

// Stat Change Watchers

var statWatchers = {};

fs.watchFile = function (filename) {
  var stat;
  var options;
  var listener;

  if ("object" == typeof arguments[1]) {
    options = arguments[1];
    listener = arguments[2];
  } else {
    options = {};
    listener = arguments[1];
  }

  if (options.persistent === undefined) options.persistent = true;
  if (options.interval === undefined) options.interval = 0;

  if (statWatchers[filename]) {
    stat = statWatchers[filename];
  } else {
    statWatchers[filename] = new binding.StatWatcher();
    stat = statWatchers[filename];
    stat.start(filename, options.persistent, options.interval);
  }
  stat.addListener("change", listener);
  return stat;
};

fs.unwatchFile = function (filename) {
  if (statWatchers[filename]) {
    stat = statWatchers[filename];
    stat.stop();
    statWatchers[filename] = undefined;
  }
};

// Realpath

var path = require('path');
var normalize = path.normalize;
var normalizeArray = path.normalizeArray;

fs.realpathSync = function (path) {
  var seen_links = {}, knownHards = {}, buf, i = 0, part, x, stats;
  if (path.charAt(0) !== '/') {
    var cwd = process.cwd().split('/');
    path = cwd.concat(path.split('/'));
    path = normalizeArray(path);
    i = cwd.length;
    buf = [].concat(cwd);
  } else {
    path = normalizeArray(path.split('/'));
    buf = [''];
  }
  for (; i<path.length; i++) {
    part = path.slice(0, i+1).join('/');
    if (part.length !== 0) {
      if (part in knownHards) {
        buf.push(path[i]);
      } else {
        stats = fs.lstatSync(part);
        if (stats.isSymbolicLink()) {
          x = stats.dev.toString(32)+":"+stats.ino.toString(32);
          if (x in seen_links)
            throw new Error("cyclic link at "+part);
          seen_links[x] = true;
          part = fs.readlinkSync(part);
          if (part.charAt(0) === '/') {
            // absolute
            path = normalizeArray(part.split('/'));
            buf = [''];
            i = 0;
          } else {
            // relative
            Array.prototype.splice.apply(path, [i, 1].concat(part.split('/')));
            part = normalizeArray(path);
            var y = 0, L = Math.max(path.length, part.length), delta;
            for (; y<L && path[y] === part[y]; y++);
            if (y !== L) {
              path = part;
              delta = i-y;
              i = y-1;
              if (delta > 0) buf.splice(y, delta);
            } else {
              i--;
            }
          }
        } else {
          buf.push(path[i]);
          knownHards[buf.join('/')] = true;
        }
      }
    }
  }
  return buf.join('/');
}


fs.realpath = function (path, callback) {
  var seen_links = {}, knownHards = {}, buf = [''], i = 0, part, x;
  if (path.charAt(0) !== '/') {
    // assumes cwd is canonical
    var cwd = process.cwd().split('/');
    path = cwd.concat(path.split('/'));
    path = normalizeArray(path);
    i = cwd.length-1;
    buf = [].concat(cwd);
  } else {
    path = normalizeArray(path.split('/'));
  }
  function done(err) {
    if (callback) {
      if (!err) callback(err, buf.join('/'));
      else callback(err);
    }
  }
  function next() {
    if (++i === path.length) return done();
    part = path.slice(0, i+1).join('/');
    if (part.length === 0) return next();
    if (part in knownHards) {
      buf.push(path[i]);
      next();
    } else {
      fs.lstat(part, function(err, stats){
        if (err) return done(err);
        if (stats.isSymbolicLink()) {
          x = stats.dev.toString(32)+":"+stats.ino.toString(32);
          if (x in seen_links)
            return done(new Error("cyclic link at "+part));
          seen_links[x] = true;
          fs.readlink(part, function(err, npart){
            if (err) return done(err);
            part = npart;
            if (part.charAt(0) === '/') {
              // absolute
              path = normalizeArray(part.split('/'));
              buf = [''];
              i = 0;
            } else {
              // relative
              Array.prototype.splice.apply(path, [i, 1].concat(part.split('/')));
              part = normalizeArray(path);
              var y = 0, L = Math.max(path.length, part.length), delta;
              for (; y<L && path[y] === part[y]; y++);
              if (y !== L) {
                path = part;
                delta = i-y;
                i = y-1; // resolve new node if needed
                if (delta > 0) buf.splice(y, delta);
              }
              else {
                i--; // resolve new node if needed
              }
            }
            next();
          }); // binding.readlink
        }
        else {
          buf.push(path[i]);
          knownHards[buf.join('/')] = true;
          next();
        }
      }); // binding.lstat
    }
  }
  next();
};

var pool;
function allocNewPool () {
  pool = new Buffer(kPoolSize);
  pool.used = 0;
}



fs.createReadStream = function(path, options) {
  return new ReadStream(path, options);
};

var ReadStream = fs.ReadStream = function(path, options) {
  events.EventEmitter.call(this);

  this.path = path;
  this.fd = null;
  this.readable = true;
  this.paused = false;

  this.flags = 'r';
  this.mode = 0666;
  this.bufferSize = 4 * 1024;

  options = options || {};

  // Mixin options into this
  var keys = Object.keys(options);
  for (var index = 0, length = keys.length; index < length; index++) {
    var key = keys[index];
    this[key] = options[key];
  }

  if (this.fd !== null) {
    return;
  }

  var self = this;
  fs.open(this.path, this.flags, this.mode, function(err, fd) {
    if (err) {
      self.emit('error', err);
      self.readable = false;
      return;
    }

    self.fd = fd;
    self.emit('open', fd);
    self._read();
  });
};
sys.inherits(ReadStream, events.EventEmitter);

fs.FileReadStream = fs.ReadStream; // support the legacy name

ReadStream.prototype.setEncoding = function (encoding) {
  var StringDecoder = require("string_decoder").StringDecoder; // lazy load
  this._decoder = new StringDecoder(encoding);
};


ReadStream.prototype._read = function () {
  var self = this;
  if (!self.readable || self.paused) return;

  if (!pool || pool.length - pool.used < kMinPoolSpace) {
    // discard the old pool. Can't add to the free list because
    // users might have refernces to slices on it.
    pool = null;
    allocNewPool();
  }

  // Grab another reference to the pool in the case that while we're in the
  // thread pool another read() finishes up the pool, and allocates a new
  // one.
  var thisPool = pool;
  var toRead = Math.min(pool.length - pool.used, this.bufferSize);
  var start = pool.used;

  function afterRead (err, bytesRead) {
    if (err) {
      self.emit('error', err);
      self.readable = false;
      return;
    }

    if (bytesRead === 0) {
      self.emit('end');
      self.destroy();
      return;
    }

    var b = thisPool.slice(start, start+bytesRead);

    // Possible optimizition here?
    // Reclaim some bytes if bytesRead < toRead?
    // Would need to ensure that pool === thisPool.

    // do not emit events if the stream is paused
    if (self.paused) {
      self.buffer = b;
      return;
    }

    // do not emit events anymore after we declared the stream unreadable
    if (!self.readable) return;

    self._emitData(b);
    self._read();
  }

  fs.read(self.fd, pool, pool.used, toRead, undefined, afterRead);
  pool.used += toRead;
};


ReadStream.prototype._emitData = function (d) {
  if (this._decoder) {
    var string = this._decoder.write(d);
    if (string.length) this.emit('data', string);
  } else {
    this.emit('data', d);
  }
};


var readStreamForceCloseWarning;

ReadStream.prototype.forceClose = function (cb) {
  if (!readStreamForceCloseWarning) {
    readStreamForceCloseWarning = "ReadStream.prototype.forceClose renamed to destroy()";
    sys.error(readStreamForceCloseWarning);
  }
  return this.destroy(cb);
};


ReadStream.prototype.destroy = function (cb) {
  var self = this;
  this.readable = false;

  function close() {
    fs.close(self.fd, function(err) {
      if (err) {
        if (cb) cb(err);
        self.emit('error', err);
        return;
      }

      if (cb) cb(null);
      self.emit('close');
    });
  }

  if (this.fd) {
    close();
  } else {
    this.addListener('open', close);
  }
};


ReadStream.prototype.pause = function() {
  this.paused = true;
};


ReadStream.prototype.resume = function() {
  this.paused = false;

  if (this.buffer) {
    this._emitData(this.buffer);
    this.buffer = null;
  }

  this._read();
};



fs.createWriteStream = function(path, options) {
  return new WriteStream(path, options);
};

var WriteStream = fs.WriteStream = function(path, options) {
  events.EventEmitter.call(this);

  this.path = path;
  this.fd = null;
  this.writeable = true;

  this.flags = 'w';
  this.encoding = 'binary';
  this.mode = 0666;

  options = options || {};

  // Mixin options into this
  var keys = Object.keys(options);
  for (var index = 0, length = keys.length; index < length; index++) {
    var key = keys[index];
    this[key] = options[key];
  }

  this.busy = false;
  this._queue = [];

  if (this.fd === null) {
    this._queue.push([fs.open, this.path, this.flags, this.mode, undefined]);
    this.flush();
  }
};
sys.inherits(WriteStream, events.EventEmitter);

fs.FileWriteStream = fs.WriteStream; // support the legacy name


WriteStream.prototype.flush = function () {
  if (this.busy) return;
  var self = this;

  var args = this._queue.shift();
  if (!args) return self.emit('drain');

  this.busy = true;

  var method = args.shift(),
      cb = args.pop();

  var self = this;

  args.push(function(err) {
    self.busy = false;

    if (err) {
      self.writeable = false;
      if (cb) {
        cb(err);
      }
      self.emit('error', err);
      return;
    }

    // stop flushing after close
    if (method === fs.close) {
      if (cb) {
        cb(null);
      }
      self.emit('close');
      return;
    }

    // save reference for file pointer
    if (method === fs.open) {
      self.fd = arguments[1];
      self.emit('open', self.fd);
    } else if (cb) {
      // write callback
      cb(null, arguments[1]);
    }

    self.flush();
  });

  // Inject the file pointer
  if (method !== fs.open) {
    args.unshift(self.fd);
  }

  method.apply(this, args);
};


WriteStream.prototype.write = function (data) {
  if (!this.writeable) {
    throw new Error('stream not writeable');
  }

  var cb;
  if (typeof(arguments[arguments.length-1]) == 'function') {
    cb = arguments[arguments.length-1];
  }

  if (Buffer.isBuffer(data)) {
    this._queue.push([fs.write, data, 0, data.length, null, cb]);
  } else {
    var encoding = 'utf8';
    if (typeof(arguments[1]) == 'string') encoding = arguments[1];
    this._queue.push([fs.write, data, undefined, encoding, cb]);
  }


  this.flush();

  return false;
};


var writeStreamCloseWarning;

WriteStream.prototype.close = function (cb) {
  if (!writeStreamCloseWarning) {
    writeStreamCloseWarning = "WriteStream.prototype.close renamed to end()";
    sys.error(writeStreamCloseWarning);
  }
  return this.end(cb);
}


WriteStream.prototype.end = function (cb) {
  this.writeable = false;
  this._queue.push([fs.close, cb]);
  this.flush();
};


var writeStreamForceCloseWarning;

WriteStream.prototype.forceClose = function (cb) {
  if (!writeStreamForceCloseWarning) {
    writeStreamForceCloseWarning = "WriteStream.prototype.forceClose renamed to destroy()";
    sys.error(writeStreamForceCloseWarning);
  }
  return this.destroy(cb);
};


WriteStream.prototype.forceClose = function (cb) {
  this.writeable = false;
  fs.close(self.fd, function(err) {
    if (err) {
      if (cb) {
        cb(err);
      }

      self.emit('error', err);
      return;
    }

    if (cb) {
      cb(null);
    }
    self.emit('close');
  });
};

