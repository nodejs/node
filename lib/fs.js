// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var util = require('util');

var binding = process.binding('fs');
var constants = process.binding('constants');
var fs = exports;
var Stream = require('stream').Stream;

var kMinPoolSpace = 128;
var kPoolSize = 40 * 1024;

fs.Stats = binding.Stats;

fs.Stats.prototype._checkModeProperty = function(property) {
  return ((this.mode & constants.S_IFMT) === property);
};

fs.Stats.prototype.isDirectory = function() {
  return this._checkModeProperty(constants.S_IFDIR);
};

fs.Stats.prototype.isFile = function() {
  return this._checkModeProperty(constants.S_IFREG);
};

fs.Stats.prototype.isBlockDevice = function() {
  return this._checkModeProperty(constants.S_IFBLK);
};

fs.Stats.prototype.isCharacterDevice = function() {
  return this._checkModeProperty(constants.S_IFCHR);
};

fs.Stats.prototype.isSymbolicLink = function() {
  return this._checkModeProperty(constants.S_IFLNK);
};

fs.Stats.prototype.isFIFO = function() {
  return this._checkModeProperty(constants.S_IFIFO);
};

fs.Stats.prototype.isSocket = function() {
  return this._checkModeProperty(constants.S_IFSOCK);
};

fs.readFile = function(path, encoding_) {
  var encoding = typeof(encoding_) === 'string' ? encoding_ : null;
  var callback = arguments[arguments.length - 1];
  if (typeof(callback) !== 'function') callback = noop;
  var readStream = fs.createReadStream(path);
  var buffers = [];
  var nread = 0;

  readStream.on('data', function(chunk) {
    buffers.push(chunk);
    nread += chunk.length;
  });

  readStream.on('error', function(er) {
    callback(er);
    readStream.destroy();
  });

  readStream.on('end', function() {
    // copy all the buffers into one
    var buffer;
    switch (buffers.length) {
      case 0: buffer = new Buffer(0); break;
      case 1: buffer = buffers[0]; break;
      default: // concat together
        buffer = new Buffer(nread);
        var n = 0;
        buffers.forEach(function(b) {
          var l = b.length;
          b.copy(buffer, n, 0, l);
          n += l;
        });
        break;
    }
    if (encoding) {
      try {
        buffer = buffer.toString(encoding);
      } catch (er) {
        return callback(er);
      }
    }
    callback(null, buffer);
  });
};

fs.readFileSync = function(path, encoding) {
  var fd = fs.openSync(path, constants.O_RDONLY, 0666);
  var buffer = new Buffer(4048);
  var buffers = [];
  var nread = 0;
  var lastRead = 0;

  do {
    if (lastRead) {
      buffer._bytesRead = lastRead;
      nread += lastRead;
      buffers.push(buffer);
    }
    var buffer = new Buffer(4048);
    lastRead = fs.readSync(fd, buffer, 0, buffer.length, null);
  } while (lastRead > 0);

  fs.closeSync(fd);

  if (buffers.length > 1) {
    var offset = 0;
    var i;
    buffer = new Buffer(nread);
    buffers.forEach(function(i) {
      if (!i._bytesRead) return;
      i.copy(buffer, offset, 0, i._bytesRead);
      offset += i._bytesRead;
    });
  } else if (buffers.length) {
    // buffers has exactly 1 (possibly zero length) buffer, so this should
    // be a shortcut
    buffer = buffers[0].slice(0, buffers[0]._bytesRead);
  } else {
    buffer = new Buffer(0);
  }

  if (encoding) buffer = buffer.toString(encoding);
  return buffer;
};


// Used by binding.open and friends
function stringToFlags(flag) {
  // Only mess with strings
  if (typeof flag !== 'string') {
    return flag;
  }
  switch (flag) {
    case 'r':
      return constants.O_RDONLY;

    case 'r+':
      return constants.O_RDWR;

    case 'w':
      return constants.O_CREAT | constants.O_TRUNC | constants.O_WRONLY;

    case 'w+':
      return constants.O_CREAT | constants.O_TRUNC | constants.O_RDWR;

    case 'a':
      return constants.O_APPEND | constants.O_CREAT | constants.O_WRONLY;

    case 'a+':
      return constants.O_APPEND | constants.O_CREAT | constants.O_RDWR;

    default:
      throw new Error('Unknown file open flag: ' + flag);
  }
}

function noop() {}

// Yes, the follow could be easily DRYed up but I provide the explicit
// list to make the arguments clear.

fs.close = function(fd, callback) {
  binding.close(fd, callback || noop);
};

fs.closeSync = function(fd) {
  return binding.close(fd);
};

function modeNum(m, def) {
  switch(typeof m) {
    case 'number': return m;
    case 'string': return parseInt(m, 8);
    default:
      if (def) {
        return modeNum(def);
      } else {
        return undefined;
      }
  }
}

fs.open = function(path, flags, mode, callback) {
  callback = arguments[arguments.length - 1];
  if (typeof(callback) !== 'function') {
    callback = noop;
  }

  mode = modeNum(mode, '0666');

  binding.open(path, stringToFlags(flags), mode, callback);
};

fs.openSync = function(path, flags, mode) {
  mode = modeNum(mode, '0666');
  return binding.open(path, stringToFlags(flags), mode);
};

fs.read = function(fd, buffer, offset, length, position, callback) {
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

      var str = (bytesRead > 0) ? buffer.toString(encoding, 0, bytesRead) : '';

      (cb)(err, str, bytesRead);
    };
  }

  function wrapper(err, bytesRead) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback && callback(err, bytesRead || 0, buffer);
  }

  binding.read(fd, buffer, offset, length, position, wrapper);
};

fs.readSync = function(fd, buffer, offset, length, position) {
  var legacy = false;
  if (!Buffer.isBuffer(buffer)) {
    // legacy string interface (fd, length, position, encoding, callback)
    legacy = true;
    var encoding = arguments[3];
    position = arguments[2];
    length = arguments[1];
    buffer = new Buffer(length);

    offset = 0;
  }

  var r = binding.read(fd, buffer, offset, length, position);
  if (!legacy) {
    return r;
  }

  var str = (r > 0) ? buffer.toString(encoding, 0, r) : '';
  return [str, r];
};

fs.write = function(fd, buffer, offset, length, position, callback) {
  if (!Buffer.isBuffer(buffer)) {
    // legacy string interface (fd, data, position, encoding, callback)
    callback = arguments[4];
    position = arguments[2];

    buffer = new Buffer('' + arguments[1], arguments[3]);
    offset = 0;
    length = buffer.length;
  }

  if (!length) {
    if (typeof callback == 'function') {
      process.nextTick(function() {
        callback(undefined, 0);
      });
    }
    return;
  }

  function wrapper(err, written) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback && callback(err, written || 0, buffer);
  }

  binding.write(fd, buffer, offset, length, position, wrapper);
};

fs.writeSync = function(fd, buffer, offset, length, position) {
  if (!Buffer.isBuffer(buffer)) {
    // legacy string interface (fd, data, position, encoding)
    position = arguments[2];

    buffer = new Buffer('' + arguments[1], arguments[3]);
    offset = 0;
    length = buffer.length;
  }
  if (!length) return 0;

  return binding.write(fd, buffer, offset, length, position);
};

fs.rename = function(oldPath, newPath, callback) {
  binding.rename(oldPath, newPath, callback || noop);
};

fs.renameSync = function(oldPath, newPath) {
  return binding.rename(oldPath, newPath);
};

fs.truncate = function(fd, len, callback) {
  binding.truncate(fd, len, callback || noop);
};

fs.truncateSync = function(fd, len) {
  return binding.truncate(fd, len);
};

fs.rmdir = function(path, callback) {
  binding.rmdir(path, callback || noop);
};

fs.rmdirSync = function(path) {
  return binding.rmdir(path);
};

fs.fdatasync = function(fd, callback) {
  binding.fdatasync(fd, callback || noop);
};

fs.fdatasyncSync = function(fd) {
  return binding.fdatasync(fd);
};

fs.fsync = function(fd, callback) {
  binding.fsync(fd, callback || noop);
};

fs.fsyncSync = function(fd) {
  return binding.fsync(fd);
};

fs.mkdir = function(path, mode, callback) {
  binding.mkdir(path, modeNum(mode), callback || noop);
};

fs.mkdirSync = function(path, mode) {
  return binding.mkdir(path, modeNum(mode));
};

fs.sendfile = function(outFd, inFd, inOffset, length, callback) {
  binding.sendfile(outFd, inFd, inOffset, length, callback || noop);
};

fs.sendfileSync = function(outFd, inFd, inOffset, length) {
  return binding.sendfile(outFd, inFd, inOffset, length);
};

fs.readdir = function(path, callback) {
  binding.readdir(path, callback || noop);
};

fs.readdirSync = function(path) {
  return binding.readdir(path);
};

fs.fstat = function(fd, callback) {
  binding.fstat(fd, callback || noop);
};

fs.lstat = function(path, callback) {
  binding.lstat(path, callback || noop);
};

fs.stat = function(path, callback) {
  binding.stat(path, callback || noop);
};

fs.fstatSync = function(fd) {
  return binding.fstat(fd);
};

fs.lstatSync = function(path) {
  return binding.lstat(path);
};

fs.statSync = function(path) {
  return binding.stat(path);
};

fs.readlink = function(path, callback) {
  binding.readlink(path, callback || noop);
};

fs.readlinkSync = function(path) {
  return binding.readlink(path);
};

fs.symlink = function(destination, path, callback) {
  binding.symlink(destination, path, callback || noop);
};

fs.symlinkSync = function(destination, path) {
  return binding.symlink(destination, path);
};

fs.link = function(srcpath, dstpath, callback) {
  binding.link(srcpath, dstpath, callback || noop);
};

fs.linkSync = function(srcpath, dstpath) {
  return binding.link(srcpath, dstpath);
};

fs.unlink = function(path, callback) {
  binding.unlink(path, callback || noop);
};

fs.unlinkSync = function(path) {
  return binding.unlink(path);
};

fs.chmod = function(path, mode, callback) {
  binding.chmod(path, modeNum(mode), callback || noop);
};

fs.chmodSync = function(path, mode) {
  return binding.chmod(path, modeNum(mode));
};

fs.chown = function(path, uid, gid, callback) {
  binding.chown(path, uid, gid, callback || noop);
};

fs.chownSync = function(path, uid, gid) {
  return binding.chown(path, uid, gid);
};

function writeAll(fd, buffer, offset, length, callback) {
  // write(fd, buffer, offset, length, position, callback)
  fs.write(fd, buffer, offset, length, offset, function(writeErr, written) {
    if (writeErr) {
      fs.close(fd, function() {
        if (callback) callback(writeErr);
      });
    } else {
      if (written === length) {
        fs.close(fd, callback);
      } else {
        writeAll(fd, buffer, offset + written, length - written, callback);
      }
    }
  });
}

fs.writeFile = function(path, data, encoding_, callback) {
  var encoding = (typeof(encoding_) == 'string' ? encoding_ : 'utf8');
  var callback_ = arguments[arguments.length - 1];
  var callback = (typeof(callback_) == 'function' ? callback_ : null);
  fs.open(path, 'w', 0666, function(openErr, fd) {
    if (openErr) {
      if (callback) callback(openErr);
    } else {
      var buffer = Buffer.isBuffer(data) ? data : new Buffer(data, encoding);
      writeAll(fd, buffer, 0, buffer.length, callback);
    }
  });
};

fs.writeFileSync = function(path, data, encoding) {
  var fd = fs.openSync(path, 'w');
  if (!Buffer.isBuffer(data)) {
    data = new Buffer(data, encoding || 'utf8');
  }
  var written = 0;
  var length = data.length;
  //writeSync(fd, buffer, offset, length, position)
  while (written < length) {
    written += fs.writeSync(fd, data, written, length - written, written);
  }
  fs.closeSync(fd);
};

// Stat Change Watchers

var statWatchers = {};

fs.watchFile = function(filename) {
  var stat;
  var options;
  var listener;

  if ('object' == typeof arguments[1]) {
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
  stat.addListener('change', listener);
  return stat;
};

fs.unwatchFile = function(filename) {
  var stat;
  if (statWatchers[filename]) {
    stat = statWatchers[filename];
    stat.stop();
    statWatchers[filename] = undefined;
  }
};

// Realpath
// Not using realpath(2) because it's bad.
// See: http://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html

var path = require('path'),
    normalize = path.normalize,
    isWindows = process.platform === 'win32';

if (isWindows) {
  // Node doesn't support symlinks / lstat on windows. Hence realpath is just
  // the same as path.resolve that fails if the path doesn't exists.

  // windows version
  fs.realpathSync = function realpathSync(p, cache) {
    var p = path.resolve(p);
    if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
      return cache[p];
    }
    fs.statSync(p);
    if (cache) cache[p] = p;
    return p;
  };

  // windows version
  fs.realpath = function(p, cache, cb) {
    if (typeof cb !== 'function') {
      cb = cache;
      cache = null;
    }
    var p = path.resolve(p);
    if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
      return cb(null, cache[p]);
    }
    fs.stat(p, function(err) {
      if (err) cb(err);
      if (cache) cache[p] = p;
      cb(null, p);
    });
  };


} else /* posix */ {

  // Regexp that finds the next partion of a (partial) path
  // result is [base_with_slash, base], e.g. ['somedir/', 'somedir']
  var nextPartRe = /(.*?)(?:[\/]+|$)/g;

  // posix version
  fs.realpathSync = function realpathSync(p, cache) {
    // make p is absolute
    p = path.resolve(p);

    if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
      return cache[p];
    }

    var original = p,
        seenLinks = {},
        knownHard = {};

    // current character position in p
    var pos = 0;
    // the partial path so far, including a trailing slash if any
    var current = '';
    // the partial path without a trailing slash
    var base = '';
    // the partial path scanned in the previous round, with slash
    var previous = '';

    // walk down the path, swapping out linked pathparts for their real
    // values
    // NB: p.length changes.
    while (pos < p.length) {
      // find the next part
      nextPartRe.lastIndex = pos;
      var result = nextPartRe.exec(p);
      previous = current;
      current += result[0];
      base = previous + result[1];
      pos = nextPartRe.lastIndex;

      // continue if not a symlink, or if root
      if (!base || knownHard[base] || (cache && cache[base] === base)) {
        continue;
      }

      var resolvedLink;
      if (cache && Object.prototype.hasOwnProperty.call(cache, base)) {
        // some known symbolic link.  no need to stat again.
        resolvedLink = cache[base];
      } else {
        var stat = fs.lstatSync(base);
        if (!stat.isSymbolicLink()) {
          knownHard[base] = true;
          if (cache) cache[base] = base;
          continue;
        }

        // read the link if it wasn't read before
        var id = stat.dev.toString(32) + ':' + stat.ino.toString(32);
        if (!seenLinks[id]) {
          fs.statSync(base);
          seenLinks[id] = fs.readlinkSync(base);
          resolvedLink = path.resolve(previous, seenLinks[id]);
          // track this, if given a cache.
          if (cache) cache[base] = resolvedLink;
        }
      }

      // resolve the link, then start over
      p = path.resolve(resolvedLink, p.slice(pos));
      pos = 0;
      previous = base = current = '';
    }

    if (cache) cache[original] = p;

    return p;
  };


  // posix version
  fs.realpath = function realpath(p, cache, cb) {
    if (typeof cb !== 'function') {
      cb = cache;
      cache = null;
    }

    // make p is absolute
    p = path.resolve(p);

    if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
      return cb(null, cache[p]);
    }

    var original = p,
        seenLinks = {},
        knownHard = {};

    // current character position in p
    var pos = 0;
    // the partial path so far, including a trailing slash if any
    var current = '';
    // the partial path without a trailing slash
    var base = '';
    // the partial path scanned in the previous round, with slash
    var previous = '';

    // walk down the path, swapping out linked pathparts for their real
    // values
    LOOP();
    function LOOP() {
      // stop if scanned past end of path
      if (pos >= p.length) {
        if (cache) cache[original] = p;
        return cb(null, p);
      }

      // find the next part
      nextPartRe.lastIndex = pos;
      var result = nextPartRe.exec(p);
      previous = current;
      current += result[0];
      base = previous + result[1];
      pos = nextPartRe.lastIndex;

      // continue if known to be hard or if root or in cache already.
      if (!base || knownHard[base] || (cache && cache[base] === base)) {
        return process.nextTick(LOOP);
      }

      if (cache && Object.prototype.hasOwnProperty.call(cache, base)) {
        // known symbolic link.  no need to stat again.
        return gotResolvedLink(cache[base]);
      }

      return fs.lstat(base, gotStat);
    }

    function gotStat(err, stat) {
      if (err) return cb(err);

      // if not a symlink, skip to the next path part
      if (!stat.isSymbolicLink()) {
        knownHard[base] = true;
        if (cache) cache[base] = base;
        return process.nextTick(LOOP);
      }

      // stat & read the link if not read before
      // call gotTarget as soon as the link target is known
      var id = stat.dev.toString(32) + ':' + stat.ino.toString(32);
      if (seenLinks[id]) {
        return gotTarget(null, seenLinks[id], base);
      }
      fs.stat(base, function(err) {
        if (err) return cb(err);

        fs.readlink(base, function(err, target) {
          gotTarget(err, seenLinks[id] = target);
        });
      });
    }

    function gotTarget(err, target, base) {
      if (err) return cb(err);

      var resolvedLink = path.resolve(previous, target);
      if (cache) cache[base] = resolvedLink;
      gotResolvedLink(resolvedLink);
    }

    function gotResolvedLink(resolvedLink) {

      // resolve the link, then start over
      p = path.resolve(resolvedLink, p.slice(pos));
      pos = 0;
      previous = base = current = '';

      return process.nextTick(LOOP);
    }
  };

}


var pool;

function allocNewPool() {
  pool = new Buffer(kPoolSize);
  pool.used = 0;
}



fs.createReadStream = function(path, options) {
  return new ReadStream(path, options);
};

var ReadStream = fs.ReadStream = function(path, options) {
  if (!(this instanceof ReadStream)) return new ReadStream(path, options);

  Stream.call(this);

  var self = this;

  this.path = path;
  this.fd = null;
  this.readable = true;
  this.paused = false;

  this.flags = 'r';
  this.mode = parseInt('0666', 8);
  this.bufferSize = 64 * 1024;

  options = options || {};

  // Mixin options into this
  var keys = Object.keys(options);
  for (var index = 0, length = keys.length; index < length; index++) {
    var key = keys[index];
    this[key] = options[key];
  }

  if (this.encoding) this.setEncoding(this.encoding);

  if (this.start !== undefined) {
    if (this.end === undefined) {
      this.end = Infinity;
    }

    if (this.start > this.end) {
      this.emit('error', new Error('start must be <= end'));
    } else {
      this._firstRead = true;
    }
  }

  if (this.fd !== null) {
    return;
  }

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
util.inherits(ReadStream, Stream);

fs.FileReadStream = fs.ReadStream; // support the legacy name

ReadStream.prototype.setEncoding = function(encoding) {
  var StringDecoder = require('string_decoder').StringDecoder; // lazy load
  this._decoder = new StringDecoder(encoding);
};


ReadStream.prototype._read = function() {
  var self = this;
  if (!self.readable || self.paused || self.reading) return;

  self.reading = true;

  if (!pool || pool.length - pool.used < kMinPoolSpace) {
    // discard the old pool. Can't add to the free list because
    // users might have refernces to slices on it.
    pool = null;
    allocNewPool();
  }

  if (self.start !== undefined && self._firstRead) {
    self.pos = self.start;
    self._firstRead = false;
  }

  // Grab another reference to the pool in the case that while we're in the
  // thread pool another read() finishes up the pool, and allocates a new
  // one.
  var thisPool = pool;
  var toRead = Math.min(pool.length - pool.used, this.bufferSize);
  var start = pool.used;

  if (this.pos !== undefined) {
    toRead = Math.min(this.end - this.pos + 1, toRead);
  }

  function afterRead(err, bytesRead) {
    self.reading = false;
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

    var b = thisPool.slice(start, start + bytesRead);

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

  fs.read(self.fd, pool, pool.used, toRead, self.pos, afterRead);

  if (self.pos !== undefined) {
    self.pos += toRead;
  }
  pool.used += toRead;
};


ReadStream.prototype._emitData = function(d) {
  if (this._decoder) {
    var string = this._decoder.write(d);
    if (string.length) this.emit('data', string);
  } else {
    this.emit('data', d);
  }
};


ReadStream.prototype.destroy = function(cb) {
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

  // hasn't opened yet.
  if (null == this.fd) return;

  this._read();
};



fs.createWriteStream = function(path, options) {
  return new WriteStream(path, options);
};

var WriteStream = fs.WriteStream = function(path, options) {
  if (!(this instanceof WriteStream)) return new WriteStream(path, options);

  Stream.call(this);

  this.path = path;
  this.fd = null;
  this.writable = true;

  this.flags = 'w';
  this.encoding = 'binary';
  this.mode = parseInt('0666', 8);

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
util.inherits(WriteStream, Stream);

fs.FileWriteStream = fs.WriteStream; // support the legacy name

WriteStream.prototype.flush = function() {
  if (this.busy) return;
  var self = this;

  var args = this._queue.shift();
  if (!args) {
    if (this.drainable) { self.emit('drain'); }
    return;
  }

  this.busy = true;

  var method = args.shift(),
      cb = args.pop();

  var self = this;

  args.push(function(err) {
    self.busy = false;

    if (err) {
      self.writable = false;
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

WriteStream.prototype.write = function(data) {
  if (!this.writable) {
    throw new Error('stream not writable');
  }

  this.drainable = true;

  var cb;
  if (typeof(arguments[arguments.length - 1]) == 'function') {
    cb = arguments[arguments.length - 1];
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

WriteStream.prototype.end = function(data, encoding, cb) {
  if (typeof(data) === 'function') {
    cb = data;
  } else if (typeof(encoding) === 'function') {
    cb = encoding;
    this.write(data);
  } else if (arguments.length > 0) {
    this.write(data, encoding);
  }
  this.writable = false;
  this._queue.push([fs.close, cb]);
  this.flush();
};

WriteStream.prototype.destroy = function(cb) {
  var self = this;
  this.writable = false;

  function close() {
    fs.close(self.fd, function(err) {
      if (err) {
        if (cb) { cb(err); }
        self.emit('error', err);
        return;
      }

      if (cb) { cb(null); }
      self.emit('close');
    });
  }

  if (this.fd) {
    close();
  } else {
    this.addListener('open', close);
  }
};

// There is no shutdown() for files.
WriteStream.prototype.destroySoon = WriteStream.prototype.end;

