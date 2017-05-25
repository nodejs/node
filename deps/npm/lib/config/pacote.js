'use strict'

const BB = require('bluebird')

const cp = require('child_process')
const npm = require('../npm')
const log = require('npmlog')
const packToStream = require('../utils/tar').packToStream
const path = require('path')
const pipe = BB.promisify(require('mississippi').pipe)
const readJson = BB.promisify(require('read-package-json'))
const PassThrough = require('stream').PassThrough

let effectiveOwner

module.exports = pacoteOpts
function pacoteOpts (moreOpts) {
  const ownerStats = calculateOwner()
  const opts = {
    cache: path.join(npm.config.get('cache'), '_cacache'),
    defaultTag: npm.config.get('tag'),
    dirPacker: prepareAndPack,
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
    Object.assign(opts, ownerStats, {
      cacheUid: ownerStats.uid,
      cacheGid: ownerStats.gid
    })
  }

  npm.config.keys.forEach(function (k) {
    if (k[0] === '/' && k.match(/.*:_authToken$/)) {
      if (!opts.auth) { opts.auth = {} }
      opts.auth[k.replace(/:_authToken$/, '')] = {
        token: npm.config.get(k)
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

const PASSTHROUGH_OPTS = [
  'always-auth',
  'auth-type',
  'ca',
  'cafile',
  'cert',
  'git',
  'local-address',
  'maxsockets',
  'offline',
  'prefer-offline',
  'prefer-online',
  'proxy',
  'https-proxy',
  'registry',
  'send-metrics',
  'sso-poll-frequency',
  'sso-type',
  'strict-ssl'
]

function prepareAndPack (manifest, dir) {
  const stream = new PassThrough()
  readJson(path.join(dir, 'package.json')).then((pkg) => {
    if (pkg.scripts && pkg.scripts.prepare) {
      log.verbose('prepareGitDep', `${manifest._spec}: installing devDeps and running prepare script.`)
      const cliArgs = PASSTHROUGH_OPTS.reduce((acc, opt) => {
        if (npm.config.get(opt, 'cli') != null) {
          acc.push(`--${opt}=${npm.config.get(opt)}`)
        }
        return acc
      }, [])
      const child = cp.spawn(process.env.NODE || process.execPath, [
        require.main.filename,
        'install',
        '--ignore-prepublish',
        '--no-progress',
        '--no-save'
      ].concat(cliArgs), {
        cwd: dir,
        env: process.env
      })
      let errData = []
      let errDataLen = 0
      let outData = []
      let outDataLen = 0
      child.stdout.on('data', (data) => {
        outData.push(data)
        outDataLen += data.length
        log.gauge.pulse('preparing git package')
      })
      child.stderr.on('data', (data) => {
        errData.push(data)
        errDataLen += data.length
        log.gauge.pulse('preparing git package')
      })
      return BB.fromNode((cb) => {
        child.on('error', cb)
        child.on('exit', (code, signal) => {
          if (code > 0) {
            const err = new Error(`${signal}: npm exited with code ${code} while attempting to build ${manifest._requested}. Clone the repository manually and run 'npm install' in it for more information.`)
            err.code = code
            err.signal = signal
            cb(err)
          } else {
            cb()
          }
        })
      }).then(() => {
        if (outDataLen > 0) log.silly('prepareGitDep', '1>', Buffer.concat(outData, outDataLen).toString())
        if (errDataLen > 0) log.silly('prepareGitDep', '2>', Buffer.concat(errData, errDataLen).toString())
      }, (err) => {
        if (outDataLen > 0) log.error('prepareGitDep', '1>', Buffer.concat(outData, outDataLen).toString())
        if (errDataLen > 0) log.error('prepareGitDep', '2>', Buffer.concat(errData, errDataLen).toString())
        throw err
      })
    }
  }).then(() => {
    return pipe(packToStream(manifest, dir), stream)
  }).catch((err) => stream.emit('error', err))
  return stream
}
