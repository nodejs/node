var path = require('path')
var chain = require('slide').chain
var table = require('text-table')
var color = require('ansicolors')
var styles = require('ansistyles')
var semver = require('semver')
var npm = require('./npm.js')
var log = require('npmlog')
var ansiTrim = require('./utils/ansi-trim.js')
var output = require('./utils/output.js')
var defaultRegistry = require('./config/defaults.js').defaults.registry
var checkPing = require('./doctor/check-ping.js')
var getGitPath = require('./doctor/get-git-path.js')
var checksumCachedFiles = require('./doctor/checksum-cached-files.js')
var checkFilesPermission = require('./doctor/check-files-permission.js')
var getLatestNodejsVersion = require('./doctor/get-latest-nodejs-version.js')
var getLatestNpmVersion = require('./doctor/get-latest-npm-version')
var globalNodeModules = path.join(npm.config.globalPrefix, 'lib', 'node_modules')
var localNodeModules = path.join(npm.config.localPrefix, 'node_modules')

module.exports = doctor

doctor.usage = 'npm doctor'

function doctor (args, silent, cb) {
  args = args || {}
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  var actionsToRun = [
    [checkPing],
    [getLatestNpmVersion],
    [getLatestNodejsVersion, args['node-url']],
    [getGitPath],
    [checkFilesPermission, npm.cache, 6],
    [checkFilesPermission, globalNodeModules, 4],
    [checkFilesPermission, localNodeModules, 6],
    [checksumCachedFiles]
  ]

  log.info('doctor', 'Running checkup')
  chain(actionsToRun, function (stderr, stdout) {
    if (stderr && stderr.message !== 'not found: git') return cb(stderr)
    var outHead = ['Check', 'Value', 'Recommendation']
    var list = makePretty(stdout)
    var outBody = list

    if (npm.color) {
      outHead = outHead.map(function (item) {
        return styles.underline(item)
      })
      outBody = outBody.map(function (item) {
        if (item[2]) {
          item[0] = color.red(item[0])
          item[2] = color.magenta(item[2])
        }
        return item
      })
    }

    var outTable = [outHead].concat(outBody)
    var tableOpts = {
      stringLength: function (s) { return ansiTrim(s).length }
    }

    if (!silent) output(table(outTable, tableOpts))

    cb(null, list)
  })
}

function makePretty (p) {
  var ping = p[0] ? 'ok' : 'notOk'
  var npmLTS = p[1]
  var nodeLTS = p[2].replace('v', '')
  var whichGit = p[3] || 'not installed'
  var readbleCaches = p[4] ? 'ok' : 'notOk'
  var executableGlobalModules = p[5] ? 'ok' : 'notOk'
  var executableLocalModules = p[6] ? 'ok' : 'notOk'
  var checksumCachedFiles = p[7] ? 'ok' : 'notOk'
  var npmV = npm.version
  var nodeV = process.version.replace('v', '')
  var registry = npm.config.get('registry')
  var list = [
    ['npm ping', ping],
    ['npm -v', 'v' + npmV],
    ['node -v', 'v' + nodeV],
    ['npm config get registry', registry],
    ['which git', whichGit],
    ['Perms check on cached files', readbleCaches],
    ['Perms check on global node_modules', executableGlobalModules],
    ['Perms check on local node_modules', executableLocalModules],
    ['Checksum cached files', checksumCachedFiles]
  ]

  if (ping !== 'ok') list[0][2] = 'Check your internet connection'
  if (!semver.satisfies(npmV, '>=' + npmLTS)) list[1][2] = 'Use npm v' + npmLTS
  if (!semver.satisfies(nodeV, '>=' + nodeLTS)) list[2][2] = 'Use node v' + nodeLTS
  if (registry !== defaultRegistry) list[3][2] = 'Try `npm config set registry ' + defaultRegistry + '`'
  if (whichGit === 'not installed') list[4][2] = 'Install git and ensure it\'s in your PATH.'
  if (readbleCaches !== 'ok') list[5][2] = 'Check the permissions of your files in ' + npm.config.get('cache')
  if (executableGlobalModules !== 'ok') list[6][2] = globalNodeModules + ' must be readable and writable by the current user.'
  if (executableLocalModules !== 'ok') list[7][2] = localNodeModules + ' must be readable and writable by the current user.'
  if (checksumCachedFiles !== 'ok') list[8][2] = 'You have some broken packages in your cache.'

  return list
}
