// prune extraneous packages.

module.exports = prune
module.exports.Pruner = Pruner

prune.usage = 'npm prune [[<@scope>/]<pkg>...] [--production]'

var npm = require('./npm.js')
var log = require('npmlog')
var util = require('util')
var moduleName = require('./utils/module-name.js')
var Installer = require('./install.js').Installer
var isExtraneous = require('./install/is-extraneous.js')
var isOnlyDev = require('./install/is-only-dev.js')
var removeDeps = require('./install/deps.js').removeDeps
var loadExtraneous = require('./install/deps.js').loadExtraneous
var chain = require('slide').chain
var computeMetadata = require('./install/deps.js').computeMetadata

prune.completion = require('./utils/completion/installed-deep.js')

function prune (args, cb) {
  var dryrun = !!npm.config.get('dry-run')
  new Pruner('.', dryrun, args).run(cb)
}

function Pruner (where, dryrun, args) {
  Installer.call(this, where, dryrun, args)
}
util.inherits(Pruner, Installer)

Pruner.prototype.loadAllDepsIntoIdealTree = function (cb) {
  log.silly('uninstall', 'loadAllDepsIntoIdealTree')

  var cg = this.progress['loadIdealTree:loadAllDepsIntoIdealTree']
  var steps = []

  computeMetadata(this.idealTree)
  var self = this
  var excludeDev = npm.config.get('production') || /^prod(uction)?$/.test(npm.config.get('only'))
  function shouldPrune (child) {
    if (isExtraneous(child)) return true
    if (!excludeDev) return false
    return isOnlyDev(child)
  }
  function getModuleName (child) {
    // wrapping because moduleName doesn't like extra args and we're called
    // from map.
    return moduleName(child)
  }
  function matchesArg (name) {
    return self.args.length === 0 || self.args.indexOf(name) !== -1
  }
  function nameObj (name) {
    return {name: name}
  }
  var toPrune = this.idealTree.children.filter(shouldPrune).map(getModuleName).filter(matchesArg).map(nameObj)

  steps.push(
    [removeDeps, toPrune, this.idealTree, null],
    [loadExtraneous, this.idealTree, cg.newGroup('loadExtraneous')])
  chain(steps, cb)
}

Pruner.prototype.runPreinstallTopLevelLifecycles = function (cb) { cb() }
Pruner.prototype.runPostinstallTopLevelLifecycles = function (cb) { cb() }
