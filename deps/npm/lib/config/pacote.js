'use strict'

const Buffer = require('safe-buffer').Buffer

const crypto = require('crypto')
const npm = require('../npm')
const log = require('npmlog')
let pack
const path = require('path')

let effectiveOwner

const npmSession = crypto.randomBytes(8).toString('hex')
log.verbose('npm-session', npmSession)

module.exports = pacoteOpts
function pacoteOpts (moreOpts) {
  if (!pack) {
    pack = require('../pack.js')
  }
  const ownerStats = calculateOwner()
  const opts = {
    cache: path.join(npm.config.get('cache'), '_cacache'),
    ca: npm.config.get('ca'),
    cert: npm.config.get('cert'),
    defaultTag: npm.config.get('tag'),
    dirPacker: pack.packGitDep,
    hashAlgorithm: 'sha1',
    includeDeprecated: false,
    key: npm.config.get('key'),
    localAddress: npm.config.get('local-address'),
    log: log,
    maxAge: npm.config.get('cache-min'),
    maxSockets: npm.config.get('maxsockets'),
    npmSession: npmSession,
    offline: npm.config.get('offline'),
    preferOffline: npm.config.get('prefer-offline') || npm.config.get('cache-min') > 9999,
    preferOnline: npm.config.get('prefer-online') || npm.config.get('cache-max') <= 0,
    projectScope: npm.projectScope,
    proxy: npm.config.get('https-proxy') || npm.config.get('proxy'),
    noProxy: npm.config.get('no-proxy'),
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
    userAgent: npm.config.get('user-agent'),

    dmode: npm.modes.exec,
    fmode: npm.modes.file,
    umask: npm.modes.umask
  }

  if (ownerStats.uid != null || ownerStats.gid != null) {
    Object.assign(opts, ownerStats)
  }

  npm.config.keys.forEach(function (k) {
    const authMatchGlobal = k.match(
      /^(_authToken|username|_password|password|email|always-auth|_auth)$/
    )
    const authMatchScoped = k[0] === '/' && k.match(
      /(.*):(_authToken|username|_password|password|email|always-auth|_auth)$/
    )

    // if it matches scoped it will also match global
    if (authMatchGlobal || authMatchScoped) {
      let nerfDart = null
      let key = null
      let val = null

      if (!opts.auth) { opts.auth = {} }

      if (authMatchScoped) {
        nerfDart = authMatchScoped[1]
        key = authMatchScoped[2]
        val = npm.config.get(k)
        if (!opts.auth[nerfDart]) {
          opts.auth[nerfDart] = {
            alwaysAuth: !!npm.config.get('always-auth')
          }
        }
      } else {
        key = authMatchGlobal[1]
        val = npm.config.get(k)
        opts.auth.alwaysAuth = !!npm.config.get('always-auth')
      }

      const auth = authMatchScoped ? opts.auth[nerfDart] : opts.auth
      if (key === '_authToken') {
        auth.token = val
      } else if (key.match(/password$/i)) {
        auth.password =
        // the config file stores password auth already-encoded. pacote expects
        // the actual username/password pair.
        Buffer.from(val, 'base64').toString('utf8')
      } else if (key === 'always-auth') {
        auth.alwaysAuth = val === 'false' ? false : !!val
      } else {
        auth[key] = val
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
