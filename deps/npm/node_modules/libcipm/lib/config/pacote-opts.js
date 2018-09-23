'use strict'

const Buffer = require('safe-buffer').Buffer

const crypto = require('crypto')
const path = require('path')

let effectiveOwner

const npmSession = crypto.randomBytes(8).toString('hex')

module.exports = pacoteOpts
function pacoteOpts (npmOpts, moreOpts) {
  const ownerStats = calculateOwner()
  const opts = {
    cache: path.join(npmOpts.get('cache'), '_cacache'),
    ca: npmOpts.get('ca'),
    cert: npmOpts.get('cert'),
    git: npmOpts.get('git'),
    key: npmOpts.get('key'),
    localAddress: npmOpts.get('local-address'),
    loglevel: npmOpts.get('loglevel'),
    maxSockets: +(npmOpts.get('maxsockets') || 15),
    npmSession: npmSession,
    offline: npmOpts.get('offline'),
    projectScope: moreOpts.rootPkg && getProjectScope(moreOpts.rootPkg.name),
    proxy: npmOpts.get('https-proxy') || npmOpts.get('proxy'),
    refer: 'cipm',
    registry: npmOpts.get('registry'),
    retry: {
      retries: npmOpts.get('fetch-retries'),
      factor: npmOpts.get('fetch-retry-factor'),
      minTimeout: npmOpts.get('fetch-retry-mintimeout'),
      maxTimeout: npmOpts.get('fetch-retry-maxtimeout')
    },
    strictSSL: npmOpts.get('strict-ssl'),
    userAgent: npmOpts.get('user-agent'),

    dmode: parseInt('0777', 8) & (~npmOpts.get('umask')),
    fmode: parseInt('0666', 8) & (~npmOpts.get('umask')),
    umask: npmOpts.get('umask')
  }

  if (ownerStats.uid != null || ownerStats.gid != null) {
    Object.assign(opts, ownerStats)
  }

  (npmOpts.forEach ? Array.from(npmOpts.keys()) : npmOpts.keys).forEach(k => {
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
        val = npmOpts.get(k)
        if (!opts.auth[nerfDart]) {
          opts.auth[nerfDart] = {
            alwaysAuth: !!npmOpts.get('always-auth')
          }
        }
      } else {
        key = authMatchGlobal[1]
        val = npmOpts.get(k)
        opts.auth.alwaysAuth = !!npmOpts.get('always-auth')
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
      opts.scopeTargets[k.replace(/:registry$/, '')] = npmOpts.get(k)
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

function getProjectScope (pkgName) {
  const sep = pkgName.indexOf('/')
  if (sep === -1) {
    return ''
  } else {
    return pkgName.slice(0, sep)
  }
}
