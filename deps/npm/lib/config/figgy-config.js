'use strict'

const BB = require('bluebird')

const crypto = require('crypto')
const figgyPudding = require('figgy-pudding')
const log = require('npmlog')
const npm = require('../npm.js')
const pack = require('../pack.js')
const path = require('path')

const npmSession = crypto.randomBytes(8).toString('hex')
log.verbose('npm-session', npmSession)

const SCOPE_REGISTRY_REGEX = /@.*:registry$/gi
const NpmConfig = figgyPudding({}, {
  other (key) {
    return key.match(SCOPE_REGISTRY_REGEX)
  }
})

let baseConfig

module.exports = mkConfig
function mkConfig (...providers) {
  if (!baseConfig) {
    baseConfig = NpmConfig(npm.config, {
      // Add some non-npm-config opts by hand.
      cache: path.join(npm.config.get('cache'), '_cacache'),
      // NOTE: npm has some magic logic around color distinct from the config
      // value, so we have to override it here
      color: !!npm.color,
      dirPacker: pack.packGitDep,
      hashAlgorithm: 'sha1',
      includeDeprecated: false,
      log,
      'npm-session': npmSession,
      'project-scope': npm.projectScope,
      refer: npm.referer,
      dmode: npm.modes.exec,
      fmode: npm.modes.file,
      umask: npm.modes.umask,
      npmVersion: npm.version,
      tmp: npm.tmp,
      Promise: BB
    })
    const ownerStats = calculateOwner()
    if (ownerStats.uid != null || ownerStats.gid != null) {
      baseConfig = baseConfig.concat(ownerStats)
    }
  }
  let conf = baseConfig.concat(...providers)
  // Adapt some other configs if missing
  if (npm.config.get('prefer-online') === undefined) {
    conf = conf.concat({
      'prefer-online': npm.config.get('cache-max') <= 0
    })
  }
  if (npm.config.get('prefer-online') === undefined) {
    conf = conf.concat({
      'prefer-online': npm.config.get('cache-min') >= 9999
    })
  }
  return conf
}

let effectiveOwner
function calculateOwner () {
  if (!effectiveOwner) {
    effectiveOwner = { uid: 0, gid: 0 }

    // Pretty much only on windows
    if (!process.getuid) {
      return effectiveOwner
    }

    effectiveOwner.uid = +process.getuid()
    effectiveOwner.gid = +process.getgid()

    if (effectiveOwner.uid === 0) {
      if (process.env.SUDO_UID) effectiveOwner.uid = +process.env.SUDO_UID
      if (process.env.SUDO_GID) effectiveOwner.gid = +process.env.SUDO_GID
    }
  }

  return effectiveOwner
}
