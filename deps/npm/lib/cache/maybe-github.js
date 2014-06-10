var url = require("url")
  , assert = require("assert")
  , log = require("npmlog")
  , addRemoteGit = require("./add-remote-git.js")

module.exports = function maybeGithub (p, er, cb) {
  assert(typeof p === "string", "must pass package name")
  assert(er instanceof Error, "must include error")
  assert(typeof cb === "function", "must pass callback")

  var u = "git://github.com/" + p
    , up = url.parse(u)
  log.info("maybeGithub", "Attempting %s from %s", p, u)

  return addRemoteGit(u, up, true, function (er2, data) {
    if (er2) {
      var upriv = "git+ssh://git@github.com:" + p
        , uppriv = url.parse(upriv)

      log.info("maybeGithub", "Attempting %s from %s", p, upriv)

      return addRemoteGit(upriv, uppriv, false, function (er3, data) {
        if (er3) return cb(er)
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
