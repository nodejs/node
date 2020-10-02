'use strict'

const exec = require('child_process').execFile
const spawn = require('./spawn')
const npm = require('../npm.js')
const which = require('which')
const git = npm.config.get('git')
const log = require('npmlog')
const noProgressTillDone = require('./no-progress-while-running.js').tillDone
const { promisify } = require('util')

exports.spawn = spawnGit
exports.exec = promisify(execGit)
exports.chainableExec = chainableExec
exports.whichAndExec = whichAndExec

function prefixGitArgs () {
  return process.platform === 'win32' ? ['-c', 'core.longpaths=true'] : []
}

function execGit (args, options, cb) {
  log.info('git', args)
  const fullArgs = prefixGitArgs().concat(args || [])
  return exec(git, fullArgs, options, noProgressTillDone(cb))
}

function spawnGit (args, options) {
  log.info('git', args)
  // If we're already in a git command (eg, running test as an exec
  // line in an interactive rebase) then these environment variables
  // will force git to operate on the current project, instead of
  // checking out/fetching/etc. whatever the user actually intends.
  options.env = options.env || Object.keys(process.env)
    .filter(k => !/^GIT/.test(k))
    .reduce((set, k) => {
      set[k] = process.env[k]
      return set
    }, {})
  return spawn(git, prefixGitArgs().concat(args || []), options)
}

function chainableExec () {
  var args = Array.prototype.slice.call(arguments)
  return [execGit].concat(args)
}

function whichAndExec (args, options, cb) {
  // check for git
  which(git, function (err) {
    if (err) {
      err.code = 'ENOGIT'
      return cb(err)
    }

    execGit(args, options, cb)
  })
}
