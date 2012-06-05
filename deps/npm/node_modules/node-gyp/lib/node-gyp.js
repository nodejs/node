
module.exports = exports = gyp

/**
 * Module dependencies.
 */

var fs = require('graceful-fs')
  , path = require('path')
  , nopt = require('nopt')
  , child_process = require('child_process')
  , EE = require('events').EventEmitter
  , inherits = require('util').inherits
  , commands = [
      // Module build commands
        'build'
      , 'clean'
      , 'configure'
      , 'rebuild'
      // Development Header File management commands
      , 'install'
      , 'list'
      , 'remove'
    ]
  , aliases = {
        'ls': 'list'
      , 'rm': 'remove'
    }

/**
 * The `gyp` function.
 */

function gyp () {
  return new Gyp
}

function Gyp () {
  var me = this

  // set the dir where node-gyp dev files get installed
  // TODO: make this configurable?
  //       see: https://github.com/TooTallNate/node-gyp/issues/21
  var homeDir = process.env.HOME || process.env.USERPROFILE
  this.devDir = path.resolve(homeDir, '.node-gyp')

  this.commands = {}

  commands.forEach(function (command) {
    me.commands[command] = function (argv, callback) {
      me.verbose('command', command, argv)
      return require('./' + command)(me, argv, callback)
    }
  })

  Object.keys(aliases).forEach(function (alias) {
    me.commands[alias] = me.commands[aliases[alias]]
  })
}
inherits(Gyp, EE)
exports.Gyp = Gyp
var proto = Gyp.prototype

/**
 * Export the contents of the package.json.
 */

proto.package = require('../package')

/**
 * nopt configuration definitions
 */

proto.configDefs = {
    help: Boolean    // everywhere
  , arch: String     // 'configure'
  , debug: Boolean   // 'build'
  , directory: String // bin
  , msvs_version: String // 'configure'
  , ensure: Boolean  // 'install'
  , verbose: Boolean // everywhere
  , solution: String // 'build' (windows only)
  , proxy: String // 'install'
  , nodedir: String // 'configure'
}

/**
 * nopt shorthands
 */

proto.shorthands = {
    release: '--no-debug'
  , C: '--directory'
  , d: '--debug'
}

/**
 * Parses the given argv array and sets the 'opts',
 * 'argv' and 'command' properties.
 */

proto.parseArgv = function parseOpts (argv) {
  this.opts = nopt(this.configDefs, this.shorthands, argv)
  this.argv = this.opts.argv.remain.slice()

  var commands = []
  this.argv.slice().forEach(function (arg) {
    if (arg in this.commands) {
      this.argv.splice(this.argv.indexOf(arg), 1)
      commands.push(arg)
    }
  }, this)

  this.todo = commands

  // support for inheriting config env variables from npm
  var npm_config_prefix = 'npm_config_'
  Object.keys(process.env).forEach(function (name) {
    if (name.indexOf(npm_config_prefix) !== 0) return
    var val = process.env[name]
    if (name === npm_config_prefix + 'loglevel') {
      // "loglevel" is a special case; check for "verbose"
      if (val === 'verbose') {
        this.opts.verbose = true
      }
    } else {
      // take the config name and check if it's one that node-gyp cares about
      name = name.substring(npm_config_prefix.length)
      if (name in this.configDefs) {
        this.opts[name] = val
      }
    }
  }, this)
}

/**
 * Spawns a child process and emits a 'spawn' event.
 */

proto.spawn = function spawn (command, args, opts) {
  opts || (opts = {})
  if (!opts.silent && !opts.customFds) {
    opts.customFds = [ 0, 1, 2 ]
  }
  var cp = child_process.spawn(command, args, opts)
  this.emit('spawn', command, args, cp)
  return cp
}

/**
 * Logging mechanisms.
 */

proto.info = function info () {
  var args = Array.prototype.slice.call(arguments)
  args.unshift('info')
  this.emit.apply(this, args)
}
proto.warn = function warn () {
  var args = Array.prototype.slice.call(arguments)
  args.unshift('warn')
  this.emit.apply(this, args)
}

proto.verbose = function verbose () {
  var args = Array.prototype.slice.call(arguments)
  args.unshift('verbose')
  this.emit.apply(this, args)
}

proto.usageAndExit = function usageAndExit () {
  var usage = [
      ''
    , '  Usage: node-gyp <command> [options]'
    , ''
    , '  where <command> is one of:'
    , commands.map(function (c) {
        return '    - ' + c + ' - ' + require('./' + c).usage
      }).join('\n')
    , ''
    , '  for specific command usage and options try:'
    , '    $ node-gyp <command> --help'
    , ''
    , 'node-gyp@' + this.version + '  ' + path.resolve(__dirname, '..')
  ].join('\n')

  console.log(usage)
  process.exit(4)
}

/**
 * Version number proxy.
 */

Object.defineProperty(proto, 'version', {
    get: function () {
      return this.package.version
    }
  , enumerable: true
})

