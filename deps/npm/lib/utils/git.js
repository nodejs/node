
// handle some git configuration for windows

exports.spawn = spawnGit
exports.chainableExec = chainableExec
exports.whichAndExec = whichAndExec

var exec = require("child_process").execFile
  , spawn = require("child_process").spawn
  , npm = require("../npm.js")
  , which = require("which")
  , git = npm.config.get("git")

function prefixGitArgs() {
  return process.platform === "win32" ? ["-c", "core.longpaths=true"] : []
}

function execGit(args, options, cb) {
  return exec(git, prefixGitArgs().concat(args || []), options, cb)
}

function spawnGit(args, options, cb) {
  return spawn(git, prefixGitArgs().concat(args || []), options)
}

function chainableExec() {
  var args = Array.prototype.slice.call(arguments)
  return [execGit].concat(args)
}

function whichGit(cb) {
  return which(git, cb)
}

function whichAndExec(args, options, cb) {
  // check for git
  whichGit(function (err) {
    if (err) {
      err.code = "ENOGIT"
      return cb(err)
    }

    execGit(args, options, cb)
  })
}
