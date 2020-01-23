'use strict'
/* eslint-disable camelcase */

const configCommon = require('./common-config.js')
var fs = require('graceful-fs')
var readCmdShim = require('read-cmd-shim')
var isWindows = require('../lib/utils/is-windows.js')
var Bluebird = require('bluebird')

if (isWindows) {
  var PATH = process.env.PATH ? 'PATH' : 'Path'
  process.env[PATH] += ';C:\\Program Files\\Git\\mingw64\\libexec\\git-core'
}

// remove any git envs so that we don't mess with the main repo
// when running git subprocesses in tests
Object.keys(process.env).filter(k => /^GIT/.test(k)).forEach(
  k => delete process.env[k]
)

// cheesy hackaround for test deps (read: nock) that rely on setImmediate
if (!global.setImmediate || !require('timers').setImmediate) {
  require('timers').setImmediate = global.setImmediate = function () {
    var args = [arguments[0], 0].concat([].slice.call(arguments, 1))
    setTimeout.apply(this, args)
  }
}

var spawn = require('child_process').spawn
const spawnSync = require('child_process').spawnSync
var path = require('path')

// space these out to help prevent collisions
const testId = 3 * (+process.env.TAP_CHILD_ID || 0)

// provide a working dir unique to each test
const main = require.main.filename
const testName = path.basename(main, '.js')
exports.pkg = path.resolve(path.dirname(main), testName)
var commonCache = path.resolve(__dirname, 'npm_cache_' + testName)
exports.cache = commonCache

const mkdirp = require('mkdirp')
const rimraf = require('rimraf')
rimraf.sync(exports.pkg)
rimraf.sync(commonCache)
mkdirp.sync(exports.pkg)
mkdirp.sync(commonCache)
// if we're in sudo mode, make sure that the cache is not root-owned
const isRoot = process.getuid && process.getuid() === 0
const isSudo = isRoot && process.env.SUDO_UID && process.env.SUDO_GID
if (isSudo) {
  const sudoUid = +process.env.SUDO_UID
  const sudoGid = +process.env.SUDO_GID
  fs.chownSync(commonCache, sudoUid, sudoGid)
}

const returnCwd = path.dirname(__dirname)
const find = require('which').sync('find')
require('tap').teardown(() => {
  // work around windows folder locking
  process.chdir(returnCwd)
  process.on('exit', () => {
    try {
      if (isSudo) {
        // running tests as sudo.  ensure we didn't leave any root-owned
        // files in the cache by mistake.
        const args = [ commonCache, '-uid', '0' ]
        const found = spawnSync(find, args)
        const output = found && found.stdout && found.stdout.toString()
        if (output.length) {
          const er = new Error('Root-owned files left in cache!')
          er.testName = main
          er.files = output.trim().split('\n')
          throw er
        }
      }
      if (!process.env.NO_TEST_CLEANUP) {
        rimraf.sync(exports.pkg)
        rimraf.sync(commonCache)
      }
    } catch (e) {
      if (process.platform !== 'win32') {
        throw e
      }
    }
  })
})

var port = exports.port = 15443 + testId
exports.registry = 'http://localhost:' + port

exports.altPort = 7331 + testId

exports.gitPort = 4321 + testId

var fakeRegistry = require('./fake-registry.js')
exports.fakeRegistry = fakeRegistry

const ourenv = {}
ourenv.npm_config_loglevel = 'error'
ourenv.npm_config_progress = 'false'
ourenv.npm_config_metrics = 'false'
ourenv.npm_config_audit = 'false'

ourenv.npm_config_unsafe_perm = 'true'
ourenv.npm_config_cache = commonCache
ourenv.npm_config_userconfig = exports.npm_config_userconfig = configCommon.userconfig
ourenv.npm_config_globalconfig = exports.npm_config_globalconfig = configCommon.globalconfig
ourenv.npm_config_global_style = 'false'
ourenv.npm_config_legacy_bundling = 'false'
ourenv.npm_config_fetch_retries = '0'
ourenv.npm_config_update_notifier = 'false'
ourenv.random_env_var = 'foo'
// suppress warnings about using a prerelease version of node
ourenv.npm_config_node_version = process.version.replace(/-.*$/, '')
for (let key of Object.keys(ourenv)) process.env[key] = ourenv[key]

var bin = exports.bin = require.resolve('../bin/npm-cli.js')

var chain = require('slide').chain
var once = require('once')

var nodeBin = exports.nodeBin = process.env.npm_node_execpath || process.env.NODE || process.execPath

exports.npm = function (cmd, opts, cb) {
  if (!cb) {
    var prom = new Bluebird((resolve, reject) => {
      cb = function (err, code, stdout, stderr) {
        if (err) return reject(err)
        return resolve([code, stdout, stderr])
      }
    })
  }
  cb = once(cb)
  cmd = [bin].concat(cmd)
  opts = Object.assign({}, opts || {})

  opts.env = opts.env || process.env
  if (opts.env._storage) opts.env = Object.assign({}, opts.env._storage)
  if (!opts.env.npm_config_cache) {
    opts.env.npm_config_cache = commonCache
  }
  if (!opts.env.npm_config_unsafe_perm) {
    opts.env.npm_config_unsafe_perm = 'true'
  }
  if (!opts.env.npm_config_send_metrics) {
    opts.env.npm_config_send_metrics = 'false'
  }
  if (!opts.env.npm_config_audit) {
    opts.env.npm_config_audit = 'false'
  }

  nodeBin = opts.nodeExecPath || nodeBin

  var stdout = ''
  var stderr = ''
  var child = spawn(nodeBin, cmd, opts)

  if (child.stderr) {
    child.stderr.on('data', function (chunk) {
      stderr += chunk
    })
  }

  if (child.stdout) {
    child.stdout.on('data', function (chunk) {
      stdout += chunk
    })
  }

  child.on('error', cb)

  child.on('close', function (code) {
    cb(null, code, stdout, stderr)
  })
  return prom || child
}

exports.makeGitRepo = function (params, cb) {
  // git must be called after npm.load because it uses config
  var git = require('../lib/utils/git.js')

  var root = params.path || process.cwd()
  var user = params.user || 'PhantomFaker'
  var email = params.email || 'nope@not.real'
  var added = params.added || ['package.json']
  var message = params.message || 'stub repo'

  var opts = { cwd: root, env: { PATH: process.env.PATH || process.env.Path } }
  var commands = [
    git.chainableExec(['init'], opts),
    git.chainableExec(['config', 'user.name', user], opts),
    git.chainableExec(['config', 'user.email', email], opts),
    // don't time out tests waiting for a gpg passphrase or 2fa
    git.chainableExec(['config', 'commit.gpgSign', 'false'], opts),
    git.chainableExec(['config', 'tag.gpgSign', 'false'], opts),
    git.chainableExec(['config', 'tag.forceSignAnnotated', 'false'], opts),
    git.chainableExec(['add'].concat(added), opts),
    git.chainableExec(['commit', '-m', message], opts)
  ]

  if (Array.isArray(params.commands)) {
    commands = commands.concat(params.commands)
  }

  chain(commands, cb)
}

exports.readBinLink = function (path) {
  if (isWindows) {
    return readCmdShim.sync(path)
  } else {
    return fs.readlinkSync(path)
  }
}

exports.skipIfWindows = function (why) {
  if (!isWindows) return
  if (!why) why = 'this test not available on windows'
  require('tap').plan(0, why)
  process.exit(0)
}

exports.pendIfWindows = function (why) {
  if (!isWindows) return
  if (!why) why = 'this test is pending further changes on windows'
  require('tap').fail(' ', { todo: why, diagnostic: false })
  process.exit(0)
}

exports.withServer = cb => {
  return fakeRegistry.compat().tap(cb).then(server => server.close())
}

exports.newEnv = function () {
  return new Environment(process.env)
}

exports.emptyEnv = function () {
  const filtered = {}
  for (let key of Object.keys(process.env)) {
    if (!/^npm_/.test(key)) filtered[key] = process.env[key]
  }
  for (let key of Object.keys(ourenv)) {
    filtered[key] = ourenv[key]
  }
  return new Environment(filtered)
}

function Environment (env) {
  if (env instanceof Environment) return env.clone()

  Object.defineProperty(this, '_storage', {
    value: Object.assign({}, env)
  })
}
Environment.prototype = {}

Environment.prototype.delete = function (key) {
  var args = Array.isArray(key) ? key : arguments
  var ii
  for (ii = 0; ii < args.length; ++ii) {
    delete this._storage[args[ii]]
  }
  return this
}

Environment.prototype.clone = function () {
  return new Environment(this._storage)
}

Environment.prototype.extend = function (env) {
  var self = this.clone()
  var args = Array.isArray(env) ? env : arguments
  var ii
  for (ii = 0; ii < args.length; ++ii) {
    var arg = args[ii]
    if (!arg) continue
    Object.keys(arg).forEach(function (name) {
      self._storage[name] = arg[name]
    })
  }
  return self
}
