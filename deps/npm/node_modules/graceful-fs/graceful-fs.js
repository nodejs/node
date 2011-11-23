// this keeps a queue of opened file descriptors, and will make
// fs operations wait until some have closed before trying to open more.
var fs = require("fs")
  , FastList = require("fast-list")
  , queue = new FastList()
  , curOpen = 0
  , constants = require("constants")

exports = module.exports = fs

fs.MAX_OPEN = 256

fs._open = fs.open
fs._openSync = fs.openSync
fs._close = fs.close
fs._closeSync = fs.closeSync


// lstat on windows, missing from early 0.5 versions
if (process.platform === "win32" && !process.binding("fs").lstat) {
  fs.lstat = fs.stat
  fs.lstatSync = fs.statSync
}

// lutimes
var constants = require("constants")
if (!fs.lutimes) fs.lutimes = function (path, at, mt, cb) {
  fs.open(path, constants.O_SYMLINK, function (er, fd) {
    cb = cb || noop
    if (er) return cb(er)
    fs.futimes(fd, at, mt, function (er) {
      if (er) {
        fs.close(fd, function () {})
        return cb(er)
      }
      fs.close(fd, cb)
    })
  })
}

if (!fs.lutimesSync) fs.lutimesSync = function (path, at, mt) {
  var fd = fs.openSync(path, constants.O_SYMLINK)
  fs.futimesSync(fd, at, mt)
  fs.closeSync(fd)
}


// prevent EMFILE errors
function OpenReq (path, flags, mode, cb) {
  this.path = path
  this.flags = flags
  this.mode = mode
  this.cb = cb
}

function noop () {}

fs.open = function (path, flags, mode, cb) {
  if (typeof mode === "function") cb = mode, mode = null
  if (typeof cb !== "function") cb = noop

  if (curOpen >= fs.MAX_OPEN) {
    queue.push(new OpenReq(path, flags, mode, cb))
    setTimeout(flush)
    return
  }
  open(path, flags, mode, cb)
}

function open (path, flags, mode, cb) {
  cb = cb || noop
  curOpen ++
  fs._open(path, flags, mode, function (er, fd) {
    if (er) {
      onclose()
    }

    cb(er, fd)
  })
}

fs.openSync = function (path, flags, mode) {
  curOpen ++
  return fs._openSync(path, flags, mode)
}

function onclose () {
  curOpen --
  flush()
}

function flush () {
  while (curOpen < fs.MAX_OPEN) {
    var req = queue.shift()
    if (!req) break
    open(req.path, req.flags, req.mode, req.cb)
  }
  if (queue.length === 0) return
}

fs.close = function (fd, cb) {
  cb = cb || noop
  fs._close(fd, function (er) {
    onclose()
    cb(er)
  })
}

fs.closeSync = function (fd) {
  onclose()
  return fs._closeSync(fd)
}

// lchmod, broken prior to 0.6.2
// back-port the fix here.
if (constants.hasOwnProperty('O_SYMLINK') &&
    process.version.match(/^v0\.6\.[0-2]|^v0\.5\./)) {
  fs.lchmod = function(path, mode, callback) {
    callback = callback || noop;
    fs.open(path, constants.O_WRONLY | constants.O_SYMLINK, function(err, fd) {
      if (err) {
        callback(err);
        return;
      }
      // prefer to return the chmod error, if one occurs,
      // but still try to close, and report closing errors if they occur.
      fs.fchmod(fd, mode, function(err) {
        fs.close(fd, function(err2) {
          callback(err || err2);
        });
      });
    });
  };

  fs.lchmodSync = function(path, mode) {
    var fd = fs.openSync(path, constants.O_WRONLY | constants.O_SYMLINK);

    // prefer to return the chmod error, if one occurs,
    // but still try to close, and report closing errors if they occur.
    var err, err2;
    try {
      var ret = fs.fchmodSync(fd, mode);
    } catch (er) {
      err = er;
    }
    try {
      fs.closeSync(fd);
    } catch (er) {
      err2 = er;
    }
    if (err || err2) throw (err || err2);
    return ret;
  };
}

// lutimes, not yet implemented in node
if (constants.hasOwnProperty('O_SYMLINK') && !fs.lutimes) {
  fs.lutimes = function (path, atime, mtime, cb) {
    cb = cb || noop
    fs.open(path, constants.O_SYMLINK | constants.O_WRONLY, function (er, fd) {
      if (er) return cb(er)
      fs.futimes(fd, atime, mtime, function (er) {
        fs.close(fd, function (er2) {
          cb(er || er2)
        })
      })
    })
  }

  fs.lutimesSync = function(path, atime, mtime) {
    var fd = fs.openSync(path, constants.O_WRONLY | constants.O_SYMLINK)

    // prefer to return the chmod error, if one occurs,
    // but still try to close, and report closing errors if they occur.
    var err, err2
    try {
      var ret = fs.futimesSync(fd, atime, mtime)
    } catch (er) {
      err = er
    }
    try {
      fs.closeSync(fd)
    } catch (er) {
      err2 = er
    }
    if (err || err2) throw (err || err2)
    return ret
  }
}
