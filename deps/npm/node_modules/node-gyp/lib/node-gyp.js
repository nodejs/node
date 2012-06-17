
module.exports = exports = gyp

/**
 * Module dependencies.
 */

var fs = require('graceful-fs')
  , path = require('path')
  , nopt = require('nopt')
  , log = require('npmlog')
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

// differentiate node-gyp's logs from npm's
log.heading = 'gyp'

/**
 * The `gyp` function.
 */

function gyp () {
  return new Gyp
}

function Gyp () {
  var self = this

  // set the dir where node-gyp dev files get installed
  // TODO: make this *more* configurable?
  //       see: https://github.com/TooTallNate/node-gyp/issues/21
  var homeDir = process.env.HOME || process.env.USERPROFILE
  this.devDir = path.resolve(homeDir, '.node-gyp')

  this.commands = {}

  commands.forEach(function (command) {
    self.commands[command] = function (argv, callback) {
      log.verbose('command', command, argv)
      return require('./' + command)(self, argv, callback)
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
  , solution: String // 'build' (windows only)
  , proxy: String // 'install'
  , nodedir: String // 'configure'
  , loglevel: String // everywhere
}

/**
 * nopt shorthands
 */

proto.shorthands = {
    release: '--no-debug'
  , C: '--directory'
  , debug: '--debug'
  , silly: '--loglevel=silly'
  , verbose: '--loglevel=verbose'
}

/**
 * expose the command aliases for the bin file to use.
 */

proto.aliases = aliases

/**
 * Parses the given argv array and sets the 'opts',
 * 'argv' and 'command' properties.
 */

proto.parseArgv = function parseOpts (argv) {
  this.opts = nopt(this.configDefs, this.shorthands, argv)
  this.argv = this.opts.argv.remain.slice()

  var commands = []
  this.argv.slice().forEach(function (arg) {
    if (arg in this.commands || arg in this.aliases) {
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
      log.level = val
    } else {
      // take the config name and check if it's one that node-gyp cares about
      name = name.substring(npm_config_prefix.length)
      if (name in this.configDefs) {
        this.opts[name] = val
      }
    }
  }, this)

  if (this.opts.loglevel) {
    log.level = this.opts.loglevel
  }
  log.resume()
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
  log.info('spawn', command)
  log.info('spawn args', args)
  return cp
}

/**
 * Prints the usage instructions and then exits.
 */

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
    , 'node@' + process.versions.node
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

