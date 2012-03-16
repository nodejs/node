
module.exports = exports = gyp

/**
 * Module dependencies.
 */

var fs = require('fs')
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

  this.commands = {}
  commands.forEach(function (command) {
    me.commands[command] = function (argv, callback) {
      me.verbose('command', command, argv)
      return require('./' + command)(me, argv, callback)
    }
  })
}
inherits(Gyp, EE)
exports.Gyp = Gyp
var proto = Gyp.prototype

/**
 * Export the contents of the package.json.
 */

proto.package = require('../package')

proto.configDefs = {
    help: Boolean    // everywhere
  , arch: String     // 'configure'
  , msvs_version: String // 'configure'
  , debug: Boolean   // 'build'
  , ensure: Boolean  // 'install'
  , verbose: Boolean // everywhere
  , solution: String // 'build' (windows only)
  , proxy: String // 'install'
}

proto.shorthands = {}

proto.parseArgv = function parseOpts (argv) {
  this.opts = nopt(this.configDefs, this.shorthands, argv)
  this.argv = this.opts.argv.remain.slice()

  var command = this.argv.shift()
  this.command = aliases[command] || command
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

