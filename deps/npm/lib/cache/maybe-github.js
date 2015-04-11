var assert = require("assert")
  , log = require("npmlog")
  , addRemoteGit = require("./add-remote-git.js")
  , hosted = require("hosted-git-info")

module.exports = function maybeGithub (p, cb) {
  assert(typeof p === "string", "must pass package name")
  assert(typeof cb === "function", "must pass callback")

  var parsed = hosted.fromUrl(p)
  log.info("maybeGithub", "Attempting %s from %s", p, parsed.git())

  return addRemoteGit(parsed.git(), true, function (er, data) {
    if (er) {
      log.info("maybeGithub", "Couldn't clone %s", parsed.git())
      log.info("maybeGithub", "Now attempting %s from %s", p, parsed.ssh())

      return addRemoteGit(parsed.ssh(), false, function (er, data) {
        if (er) return cb(er)

        success(parsed.ssh(), data)
      })
    }

    success(parsed.git(), data)
  })

  function success (u, data) {
    data._from = u
    data._fromGithub = true
    return cb(null, data)
  }
}
