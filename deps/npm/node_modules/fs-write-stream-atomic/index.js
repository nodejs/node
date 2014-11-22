var fs = require('graceful-fs')
var util = require('util')
var crypto = require('crypto')

function md5hex () {
  var hash = crypto.createHash('md5')
  for (var ii=0; ii<arguments.length; ++ii) {
    hash.update(''+arguments[ii])
  }
  return hash.digest('hex')
}

var invocations = 0;
function getTmpname (filename) {
  return filename + "." + md5hex(__filename, process.pid, ++invocations)
}

module.exports = WriteStream

util.inherits(WriteStream, fs.WriteStream)
function WriteStream (path, options) {
  if (!options)
    options = {}

  if (!(this instanceof WriteStream))
    return new WriteStream(path, options)

  this.__atomicTarget = path
  this.__atomicChown = options.chown
  this.__atomicDidStuff = false
  this.__atomicTmp = getTmpname(path)

  fs.WriteStream.call(this, this.__atomicTmp, options)
}

function cleanup (er) {
  fs.unlink(this.__atomicTmp, function () {
    fs.WriteStream.prototype.emit.call(this, 'error', er)
  }.bind(this))
}

function cleanupSync (er) {
  try {
    fs.unlinkSync(this.__atomicTmp)
  } finally {
    return fs.WriteStream.prototype.emit.call(this, 'error', er)
  }
}

// When we *would* emit 'close' or 'finish', instead do our stuff
WriteStream.prototype.emit = function (ev) {
  if (ev === 'error')
    return cleanupSync(this)

  if (ev !== 'close' && ev !== 'finish')
    return fs.WriteStream.prototype.emit.apply(this, arguments)

  // We handle emitting finish and close after the rename.
  if (ev === 'close' || ev === 'finish') {
    if (!this.__atomicDidStuff) {
      atomicDoStuff.call(this, function (er) {
        if (er)
          cleanup.call(this, er)
      }.bind(this))
    }
  }
}

function atomicDoStuff(cb) {
  if (this.__atomicDidStuff)
    throw new Error('Already did atomic move-into-place')

  this.__atomicDidStuff = true
  if (this.__atomicChown) {
    var uid = this.__atomicChown.uid
    var gid = this.__atomicChown.gid
    return fs.chown(this.__atomicTmp, uid, gid, function (er) {
      if (er) return cb(er)
      moveIntoPlace.call(this, cb)
    }.bind(this))
  } else {
    moveIntoPlace.call(this, cb)
  }
}

function moveIntoPlace (cb) {
  fs.rename(this.__atomicTmp, this.__atomicTarget, function (er) {
    cb(er)
    // emit finish, and then close on the next tick
    // This makes finish/close consistent across Node versions also.
    fs.WriteStream.prototype.emit.call(this, 'finish')
    process.nextTick(function() {
      fs.WriteStream.prototype.emit.call(this, 'close')
    }.bind(this))
  }.bind(this))
}
