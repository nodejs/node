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
process.env.npm_config_loglevel = 'error'

var npm_config_cache = path.resolve(__dirname, 'npm_cache')
process.env.npm_config_cache = exports.npm_config_cache = npm_config_cache
process.env.npm_config_userconfig = exports.npm_config_userconfig = path.join(__dirname, 'fixtures', 'config', 'userconfig')
process.env.npm_config_globalconfig = exports.npm_config_globalconfig = path.join(__dirname, 'fixtures', 'config', 'globalconfig')
process.env.random_env_var = 'foo'
// suppress warnings about using a prerelease version of node
process.env.npm_config_node_version = process.version.replace(/-.*$/, '')

var bin = exports.bin = require.resolve('../bin/npm-cli.js')
var chain = require('slide').chain
var once = require('once')

exports.npm = function npm (cmd, opts, cb) {
  cb = once(cb)
  cmd = [bin].concat(cmd)
  opts = opts || {}

  opts.env = opts.env || process.env
  if (!opts.env.npm_config_cache) {
    opts.env.npm_config_cache = npm_config_cache
  }

  var stdout = ''
  var stderr = ''
  var node = process.execPath
  var child = spawn(node, cmd, opts)

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
  return child
}

exports.makeGitRepo = function makeGitRepo (params, cb) {
  // git must be called after npm.load because it uses config
  var git = require('../lib/utils/git.js')

  var root = params.path || process.cwd()
  var user = params.user || 'PhantomFaker'
  var email = params.email || 'nope@not.real'
  var added = params.added || ['package.json']
  var message = params.message || 'stub repo'

  var opts = { cwd: root, env: { PATH: process.env.PATH }}
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

exports.rmFromInShrinkwrap = function rmFromInShrinkwrap (obj) {
  for (var i in obj) {
    if (i === "from")
      delete obj[i]
    else if (i === "dependencies")
      for (var j in obj[i])
        exports.rmFromInShrinkwrap(obj[i][j])
  }
  return obj
}