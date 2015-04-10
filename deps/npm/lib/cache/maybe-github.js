var assert = require('assert')

var hosted = require('hosted-git-info')
var log = require('npmlog')

var addRemoteGit = require('./add-remote-git.js')

module.exports = function maybeGithub (p, cb) {
  assert(typeof p === 'string', 'must pass package name')
  assert(typeof cb === 'function', 'must pass callback')

  var parsed = hosted.fromUrl(p)
  log.info('maybeGithub', 'Attempting %s from %s', p, parsed.git())

  return addRemoteGit(parsed.git(), true, function (er, data) {
    if (er) {
      log.info('maybeGithub', "Couldn't clone %s", parsed.git())
      log.info('maybeGithub', 'Now attempting %s from %s', p, parsed.sshurl())

      return addRemoteGit(parsed.sshurl(), false, function (er, data) {
        if (er) return cb(er)

        success(parsed.sshurl(), data)
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
