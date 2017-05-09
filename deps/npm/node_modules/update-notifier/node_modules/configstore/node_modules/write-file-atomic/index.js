'use strict'
module.exports = writeFile
module.exports.sync = writeFileSync
module.exports._getTmpname = getTmpname // for testing

var fs = require('graceful-fs')
var chain = require('slide').chain
var MurmurHash3 = require('imurmurhash')
var extend = Object.assign || require('util')._extend

var invocations = 0
function getTmpname (filename) {
  return filename + '.' +
    MurmurHash3(__filename)
      .hash(String(process.pid))
      .hash(String(++invocations))
      .result()
}

function writeFile (filename, data, options, callback) {
  if (options instanceof Function) {
    callback = options
    options = null
  }
  if (!options) options = {}
  fs.realpath(filename, function (_, realname) {
    _writeFile(realname || filename, data, options, callback)
  })
}
function _writeFile (filename, data, options, callback) {
  var tmpfile = getTmpname(filename)

  if (options.mode && options.chown) {
    return thenWriteFile()
  } else {
    // Either mode or chown is not explicitly set
    // Default behavior is to copy it from original file
    return fs.stat(filename, function (err, stats) {
      if (err || !stats) return thenWriteFile()

      options = extend({}, options)
      if (!options.mode) {
        options.mode = stats.mode
      }
      if (!options.chown && process.getuid) {
        options.chown = { uid: stats.uid, gid: stats.gid }
      }
      return thenWriteFile()
    })
  }

  function thenWriteFile () {
    chain([
      [writeFileAsync, tmpfile, data, options.mode, options.encoding || 'utf8'],
      options.chown && [fs, fs.chown, tmpfile, options.chown.uid, options.chown.gid],
      options.mode && [fs, fs.chmod, tmpfile, options.mode],
      [fs, fs.rename, tmpfile, filename]
    ], function (err) {
      err ? fs.unlink(tmpfile, function () { callback(err) })
        : callback()
    })
  }

  // doing this instead of `fs.writeFile` in order to get the ability to
  // call `fsync`.
  function writeFileAsync (file, data, mode, encoding, cb) {
    fs.open(file, 'w', options.mode, function (err, fd) {
      if (err) return cb(err)
      if (Buffer.isBuffer(data)) {
        return fs.write(fd, data, 0, data.length, 0, syncAndClose)
      } else if (data != null) {
        return fs.write(fd, String(data), 0, String(encoding), syncAndClose)
      } else {
        return syncAndClose()
      }
      function syncAndClose (err) {
        if (err) return cb(err)
        fs.fsync(fd, function (err) {
          if (err) return cb(err)
          fs.close(fd, cb)
        })
      }
    })
  }
}

function writeFileSync (filename, data, options) {
  if (!options) options = {}
  try {
    filename = fs.realpathSync(filename)
  } catch (ex) {
    // it's ok, it'll happen on a not yet existing file
  }
  var tmpfile = getTmpname(filename)

  try {
    if (!options.mode || !options.chown) {
      // Either mode or chown is not explicitly set
      // Default behavior is to copy it from original file
      try {
        var stats = fs.statSync(filename)
        options = extend({}, options)
        if (!options.mode) {
          options.mode = stats.mode
        }
        if (!options.chown && process.getuid) {
          options.chown = { uid: stats.uid, gid: stats.gid }
        }
      } catch (ex) {
        // ignore stat errors
      }
    }

    var fd = fs.openSync(tmpfile, 'w', options.mode)
    if (Buffer.isBuffer(data)) {
      fs.writeSync(fd, data, 0, data.length, 0)
    } else if (data != null) {
      fs.writeSync(fd, String(data), 0, String(options.encoding || 'utf8'))
    }
    fs.fsyncSync(fd)
    fs.closeSync(fd)
    if (options.chown) fs.chownSync(tmpfile, options.chown.uid, options.chown.gid)
    if (options.mode) fs.chmodSync(tmpfile, options.mode)
    fs.renameSync(tmpfile, filename)
  } catch (err) {
    try { fs.unlinkSync(tmpfile) } catch (e) {}
    throw err
  }
}
