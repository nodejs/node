// only remove the thing if it's a symlink into a specific folder.
// This is a very common use-case of npm's, but not so common elsewhere.

module.exports = gentlyRm

var rimraf = require("rimraf")
  , fs = require("graceful-fs")
  , npm = require("../npm.js")
  , path = require("path")

function gentlyRm (p, gently, cb) {
  if (npm.config.get("force") || !gently) {
    return rimraf(p, cb)
  }

  gently = path.resolve(gently)

  // lstat it, see if it's a symlink.
  fs.lstat(p, function (er, s) {
    if (er) return rimraf(p, cb)
    if (!s.isSymbolicLink()) next(null, path.resolve(p))
    realish(p, next)
  })

  function next (er, rp) {
    if (rp && rp.indexOf(gently) !== 0) {
      return clobberFail(p, gently, cb)
    }
    rimraf(p, cb)
  }
}

function realish (p, cb) {
  fs.readlink(p, function (er, r) {
    if (er) return cb(er)
    return cb(null, path.resolve(path.dirname(p), r))
  })
}

function clobberFail (p, g, cb) {
  var er = new Error("Refusing to delete: "+p+" not in "+g)
  er.code = "EEXIST"
  er.path = p
  return cb(er)
}
