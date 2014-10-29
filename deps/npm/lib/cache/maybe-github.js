var assert = require("assert")
  , log = require("npmlog")
  , addRemoteGit = require("./add-remote-git.js")

module.exports = function maybeGithub (p, cb) {
  assert(typeof p === "string", "must pass package name")
  assert(typeof cb === "function", "must pass callback")

  var u = "git://github.com/" + p
  log.info("maybeGithub", "Attempting %s from %s", p, u)

  return addRemoteGit(u, true, function (er, data) {
    if (er) {
      var upriv = "git+ssh://git@github.com:" + p
      log.info("maybeGithub", "Attempting %s from %s", p, upriv)

      return addRemoteGit(upriv, false, function (er, data) {
        if (er) return cb(er)

        success(upriv, data)
      })
    }

    success(u, data)
  })

  function success (u, data) {
    data._from = u
    data._fromGithub = true
    return cb(null, data)
  }
}
