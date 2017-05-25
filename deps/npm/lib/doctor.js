'use strict'

const ansiTrim = require('./utils/ansi-trim')
const chain = require('slide').chain
const color = require('ansicolors')
const defaultRegistry = require('./config/defaults').defaults.registry
const log = require('npmlog')
const npm = require('./npm')
const output = require('./utils/output')
const path = require('path')
const semver = require('semver')
const styles = require('ansistyles')
const table = require('text-table')

// steps
const checkFilesPermission = require('./doctor/check-files-permission')
const checkPing = require('./doctor/check-ping')
const getGitPath = require('./doctor/get-git-path')
const getLatestNodejsVersion = require('./doctor/get-latest-nodejs-version')
const getLatestNpmVersion = require('./doctor/get-latest-npm-version')
const verifyCachedFiles = require('./doctor/verify-cached-files')

const globalNodeModules = path.join(npm.config.globalPrefix, 'lib', 'node_modules')
const localNodeModules = path.join(npm.config.localPrefix, 'node_modules')

module.exports = doctor

doctor.usage = 'npm doctor'

function doctor (args, silent, cb) {
  args = args || {}
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  const actionsToRun = [
    [checkPing],
    [getLatestNpmVersion],
    [getLatestNodejsVersion, args['node-url']],
    [getGitPath],
    [checkFilesPermission, npm.cache, 6],
    [checkFilesPermission, globalNodeModules, 4],
    [checkFilesPermission, localNodeModules, 6],
    [verifyCachedFiles, path.join(npm.cache, '_cacache')]
  ]

  log.info('doctor', 'Running checkup')
  chain(actionsToRun, function (stderr, stdout) {
    if (stderr && stderr.message !== 'not found: git') return cb(stderr)
    const list = makePretty(stdout)
    let outHead = ['Check', 'Value', 'Recommendation']
    let outBody = list

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

    const outTable = [outHead].concat(outBody)
    const tableOpts = {
      stringLength: function (s) { return ansiTrim(s).length }
    }

    if (!silent) output(table(outTable, tableOpts))

    cb(null, list)
  })
}

function makePretty (p) {
  const ping = p[1]
  const npmLTS = p[2]
  const nodeLTS = p[3].replace('v', '')
  const whichGit = p[4] || 'not installed'
  const readbleCaches = p[5] ? 'ok' : 'notOk'
  const executableGlobalModules = p[6] ? 'ok' : 'notOk'
  const executableLocalModules = p[7] ? 'ok' : 'notOk'
  const cacheStatus = p[8] ? `verified ${p[8].verifiedContent} tarballs` : 'notOk'
  const npmV = npm.version
  const nodeV = process.version.replace('v', '')
  const registry = npm.config.get('registry')
  const list = [
    ['npm ping', ping],
    ['npm -v', 'v' + npmV],
    ['node -v', 'v' + nodeV],
    ['npm config get registry', registry],
    ['which git', whichGit],
    ['Perms check on cached files', readbleCaches],
    ['Perms check on global node_modules', executableGlobalModules],
    ['Perms check on local node_modules', executableLocalModules],
    ['Verify cache contents', cacheStatus]
  ]

  if (p[0] !== 200) list[0][2] = 'Check your internet connection'
  if (!semver.satisfies(npmV, '>=' + npmLTS)) list[1][2] = 'Use npm v' + npmLTS
  if (!semver.satisfies(nodeV, '>=' + nodeLTS)) list[2][2] = 'Use node v' + nodeLTS
  if (registry !== defaultRegistry) list[3][2] = 'Try `npm config set registry ' + defaultRegistry + '`'
  if (whichGit === 'not installed') list[4][2] = 'Install git and ensure it\'s in your PATH.'
  if (readbleCaches !== 'ok') list[5][2] = 'Check the permissions of your files in ' + npm.config.get('cache')
  if (executableGlobalModules !== 'ok') list[6][2] = globalNodeModules + ' must be readable and writable by the current user.'
  if (executableLocalModules !== 'ok') list[7][2] = localNodeModules + ' must be readable and writable by the current user.'

  return list
}
