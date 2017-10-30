'use strict'
var fs = require('graceful-fs')
var readCmdShim = require('read-cmd-shim')
var isWindows = require('../lib/utils/is-windows.js')
var extend = Object.assign || require('util')._extend
var Bluebird = require('bluebird')

// cheesy hackaround for test deps (read: nock) that rely on setImmediate
if (!global.setImmediate || !require('timers').setImmediate) {
  require('timers').setImmediate = global.setImmediate = function () {
    var args = [arguments[0], 0].concat([].slice.call(arguments, 1))
    setTimeout.apply(this, args)
  }
}

var spawn = require('child_process').spawn
var path = require('path')

var port = exports.port = 1337
exports.registry = 'http://localhost:' + port
const ourenv = {}
ourenv.npm_config_loglevel = 'error'
ourenv.npm_config_progress = 'false'

var npm_config_cache = path.resolve(__dirname, 'npm_cache')
ourenv.npm_config_cache = exports.npm_config_cache = npm_config_cache
ourenv.npm_config_userconfig = exports.npm_config_userconfig = path.join(__dirname, 'fixtures', 'config', 'userconfig')
ourenv.npm_config_globalconfig = exports.npm_config_globalconfig = path.join(__dirname, 'fixtures', 'config', 'globalconfig')
ourenv.npm_config_global_style = 'false'
ourenv.npm_config_legacy_bundling = 'false'
ourenv.npm_config_fetch_retries = '0'
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
  opts = extend({}, opts || {})

  opts.env = opts.env || process.env
  if (opts.env._storage) opts.env = Object.assign({}, opts.env._storage)
  if (!opts.env.npm_config_cache) {
    opts.env.npm_config_cache = npm_config_cache
  }
  if (!opts.env.npm_config_send_metrics) {
    opts.env.npm_config_send_metrics = 'false'
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

  var opts = { cwd: root, env: { PATH: process.env.PATH } }
  var commands = [
    git.chainableExec(['init'], opts),
    git.chainableExec(['config', 'user.name', user], opts),
    git.chainableExec(['config', 'user.email', email], opts),
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
  console.log('1..1')
  if (!why) why = 'this test not available on windows'
  console.log('ok 1 # skip ' + why)
  process.exit(0)
}

exports.pendIfWindows = function (why) {
  if (!isWindows) return
  console.log('1..1')
  if (!why) why = 'this test is pending further changes on windows'
  console.log('not ok 1 # todo ' + why)
  process.exit(0)
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
    value: extend({}, env)
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
