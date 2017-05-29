'use strict'

const Buffer = require('safe-buffer').Buffer

const npm = require('../npm')
const log = require('npmlog')
let pack
const path = require('path')

let effectiveOwner

module.exports = pacoteOpts
function pacoteOpts (moreOpts) {
  if (!pack) {
    pack = require('../pack.js')
  }
  const ownerStats = calculateOwner()
  const opts = {
    cache: path.join(npm.config.get('cache'), '_cacache'),
    defaultTag: npm.config.get('tag'),
    dirPacker: pack.packGitDep,
    hashAlgorithm: 'sha1',
    localAddress: npm.config.get('local-address'),
    log: log,
    maxAge: npm.config.get('cache-min'),
    maxSockets: npm.config.get('maxsockets'),
    offline: npm.config.get('offline'),
    preferOffline: npm.config.get('prefer-offline') || npm.config.get('cache-min') > 9999,
    preferOnline: npm.config.get('prefer-online') || npm.config.get('cache-max') <= 0,
    projectScope: npm.projectScope,
    proxy: npm.config.get('https-proxy') || npm.config.get('proxy'),
    refer: npm.registry.refer,
    registry: npm.config.get('registry'),
    retry: {
      retries: npm.config.get('fetch-retries'),
      factor: npm.config.get('fetch-retry-factor'),
      minTimeout: npm.config.get('fetch-retry-mintimeout'),
      maxTimeout: npm.config.get('fetch-retry-maxtimeout')
    },
    scope: npm.config.get('scope'),
    strictSSL: npm.config.get('strict-ssl'),
    userAgent: npm.config.get('user-agent')
  }

  if (ownerStats.uid || ownerStats.gid) {
    Object.assign(opts, ownerStats)
  }

  npm.config.keys.forEach(function (k) {
    const authMatch = k[0] === '/' && k.match(
      /(.*):(_authToken|username|_password|password|email|always-auth)$/
    )
    if (authMatch) {
      const nerfDart = authMatch[1]
      const key = authMatch[2]
      const val = npm.config.get(k)
      if (!opts.auth) { opts.auth = {} }
      if (!opts.auth[nerfDart]) {
        opts.auth[nerfDart] = {
          alwaysAuth: !!npm.config.get('always-auth')
        }
      }
      if (key === '_authToken') {
        opts.auth[nerfDart].token = val
      } else if (key.match(/password$/i)) {
        opts.auth[nerfDart].password =
        // the config file stores password auth already-encoded. pacote expects
        // the actual username/password pair.
        Buffer.from(val, 'base64').toString('utf8')
      } else if (key === 'always-auth') {
        opts.auth[nerfDart].alwaysAuth = val === 'false' ? false : !!val
      } else {
        opts.auth[nerfDart][key] = val
      }
    }
    if (k[0] === '@') {
      if (!opts.scopeTargets) { opts.scopeTargets = {} }
      opts.scopeTargets[k.replace(/:registry$/, '')] = npm.config.get(k)
    }
  })

  Object.keys(moreOpts || {}).forEach((k) => {
    opts[k] = moreOpts[k]
  })

  return opts
}

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
