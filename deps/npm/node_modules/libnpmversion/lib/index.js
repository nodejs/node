const readJson = require('read-package-json-fast')
const version = require('./version.js')
const proclog = require('./proc-log.js')

module.exports = async (newversion, opts = {}) => {
  const {
    path = process.cwd(),
    allowSameVersion = false,
    tagVersionPrefix = 'v',
    commitHooks = true,
    gitTagVersion = true,
    signGitCommit = false,
    signGitTag = false,
    force = false,
    ignoreScripts = false,
    preid = null,
    log = proclog,
    message = 'v%s',
  } = opts

  const pkg = opts.pkg || await readJson(path + '/package.json')

  return version(newversion, {
    path,
    cwd: path,
    allowSameVersion,
    tagVersionPrefix,
    commitHooks,
    gitTagVersion,
    signGitCommit,
    signGitTag,
    force,
    ignoreScripts,
    preid,
    pkg,
    log,
    message,
  })
}
