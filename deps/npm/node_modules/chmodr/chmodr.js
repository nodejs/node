module.exports = chmodr
chmodr.sync = chmodrSync

var fs = require("fs")
var path = require("path")

function chmodr (p, mode, cb) {
  fs.lstat(p, function (er, stats) {
    if (er)
      return cb(er)
    if (stats.isSymbolicLink())
      return cb()
    if (stats.isDirectory())
      return chmodrDir(p, mode, cb)
    return fs.chmod(p, mode, cb)
  })
}

function chmodrDir (p, mode, cb) {
  fs.readdir(p, function (er, children) {
    if (er)
      return cb(er)

    if (!children.length)
      return fs.chmod(p, dirMode(mode), cb)

    var len = children.length
    var errState = null
    children.forEach(function (child) {
      chmodr(path.resolve(p, child), mode, then)
    })

    // return first error, but not until all are finished,
    // so we don't keep performing FS operations after the cb
    function then (er) {
      len = len - 1
      if (er && !errState)
        errState = er
      if (len === 0) {
        if (errState)
          return cb(errState)
        else
          return fs.chmod(p, dirMode(mode), cb)
      }
    }
  })
}

function chmodrSync (p, mode) {
  var stats = fs.lstatSync(p)
  if (stats.isSymbolicLink())
    return
  if (stats.isDirectory())
    return chmodrDirSync(p, mode)
  else
    return fs.chmodSync(p, mode)
}

function chmodrDirSync (p, mode) {
  fs.readdirSync(p).forEach(function (child) {
    chmodrSync(path.resolve(p, child), mode)
  })
  return fs.chmodSync(p, dirMode(mode))
}

// If a party has r, add x
// so that dirs are listable
function dirMode(mode) {
  if (mode & 0400)
    mode |= 0100
  if (mode & 040)
    mode |= 010
  if (mode & 04)
    mode |= 01
  return mode
}
