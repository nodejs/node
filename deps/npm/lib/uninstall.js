'use strict'
// remove a package.

module.exports = uninstall
module.exports.Uninstaller = Uninstaller

var util = require('util')
var path = require('path')
var validate = require('aproba')
var chain = require('slide').chain
var readJson = require('read-package-json')
var npm = require('./npm.js')
var Installer = require('./install.js').Installer
var getSaveType = require('./install/save.js').getSaveType
var removeDeps = require('./install/deps.js').removeDeps
var loadExtraneous = require('./install/deps.js').loadExtraneous
var log = require('npmlog')
var usage = require('./utils/usage')

uninstall.usage = usage(
  'uninstall',
  'npm uninstall [<@scope>/]<pkg>[@<version>]... [--save|--save-dev|--save-optional]'
)

uninstall.completion = require('./utils/completion/installed-shallow.js')

function uninstall (args, cb) {
  validate('AF', arguments)
  // the /path/to/node_modules/..
  var dryrun = !!npm.config.get('dry-run')

  if (args.length === 1 && args[0] === '.') args = []
  args = args.filter(function (a) {
    return path.resolve(a) !== where
  })

  var where = npm.config.get('global') || !args.length
            ? path.resolve(npm.globalDir, '..')
            : npm.prefix

  if (args.length) {
    new Uninstaller(where, dryrun, args).run(cb)
  } else {
    // remove this package from the global space, if it's installed there
    readJson(path.resolve(npm.localPrefix, 'package.json'), function (er, pkg) {
      if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
      if (er) return cb(uninstall.usage)
      new Uninstaller(where, dryrun, [pkg.name]).run(cb)
    })
  }
}

function Uninstaller (where, dryrun, args) {
  validate('SBA', arguments)
  Installer.call(this, where, dryrun, args)
}
util.inherits(Uninstaller, Installer)

Uninstaller.prototype.loadArgMetadata = function (next) {
  this.args = this.args.map(function (arg) { return {name: arg} })
  next()
}

Uninstaller.prototype.loadAllDepsIntoIdealTree = function (cb) {
  validate('F', arguments)
  log.silly('uninstall', 'loadAllDepsIntoIdealtree')
  var saveDeps = getSaveType(this.args)

  var cg = this.progress.loadAllDepsIntoIdealTree
  var steps = []

  steps.push(
    [removeDeps, this.args, this.idealTree, saveDeps, cg.newGroup('removeDeps')],
    [loadExtraneous, this.idealTree, cg.newGroup('loadExtraneous')])
  chain(steps, cb)
}

Uninstaller.prototype.runPreinstallTopLevelLifecycles = function (cb) { cb() }
Uninstaller.prototype.runPostinstallTopLevelLifecycles = function (cb) { cb() }
