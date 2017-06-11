var util = require('util')
var path = require('path')
var validate = require('aproba')
var without = require('lodash.without')
var asyncMap = require('slide').asyncMap
var chain = require('slide').chain
var npa = require('npm-package-arg')
var log = require('npmlog')
var npm = require('./npm.js')
var Installer = require('./install.js').Installer
var findRequirement = require('./install/deps.js').findRequirement
var earliestInstallable = require('./install/deps.js').earliestInstallable
var checkPermissions = require('./install/check-permissions.js')
var decomposeActions = require('./install/decompose-actions.js')
var loadExtraneous = require('./install/deps.js').loadExtraneous
var computeMetadata = require('./install/deps.js').computeMetadata
var sortActions = require('./install/diff-trees.js').sortActions
var moduleName = require('./utils/module-name.js')
var packageId = require('./utils/package-id.js')
var childPath = require('./utils/child-path.js')
var usage = require('./utils/usage')
var getRequested = require('./install/get-requested.js')

module.exports = dedupe
module.exports.Deduper = Deduper

dedupe.usage = usage(
  'dedupe',
  'npm dedupe'
)

function dedupe (args, cb) {
  validate('AF', arguments)
  // the /path/to/node_modules/..
  var where = path.resolve(npm.dir, '..')
  var dryrun = false
  if (npm.command.match(/^find/)) dryrun = true
  if (npm.config.get('dry-run')) dryrun = true
  if (dryrun && !npm.config.get('json')) npm.config.set('parseable', true)

  new Deduper(where, dryrun).run(cb)
}

function Deduper (where, dryrun) {
  validate('SB', arguments)
  Installer.call(this, where, dryrun, [])
  this.noPackageJsonOk = true
  this.topLevelLifecycles = false
}
util.inherits(Deduper, Installer)

Deduper.prototype.loadIdealTree = function (cb) {
  validate('F', arguments)
  log.silly('install', 'loadIdealTree')

  var self = this
  chain([
    [this.newTracker(this.progress.loadIdealTree, 'cloneCurrentTree')],
    [this, this.cloneCurrentTreeToIdealTree],
    [this, this.finishTracker, 'cloneCurrentTree'],

    [this.newTracker(this.progress.loadIdealTree, 'loadAllDepsIntoIdealTree', 10)],
    [ function (next) {
      loadExtraneous(self.idealTree, self.progress.loadAllDepsIntoIdealTree, next)
    } ],
    [this, this.finishTracker, 'loadAllDepsIntoIdealTree'],

    [this, andComputeMetadata(this.idealTree)]
  ], cb)
}

function andComputeMetadata (tree) {
  return function (next) {
    next(null, computeMetadata(tree))
  }
}

Deduper.prototype.generateActionsToTake = function (cb) {
  validate('F', arguments)
  log.silly('dedupe', 'generateActionsToTake')
  chain([
    [this.newTracker(log, 'hoist', 1)],
    [hoistChildren, this.idealTree, this.differences],
    [this, this.finishTracker, 'hoist'],
    [this.newTracker(log, 'sort-actions', 1)],
    [this, function (next) {
      this.differences = sortActions(this.differences)
      next()
    }],
    [this, this.finishTracker, 'sort-actions'],
    [checkPermissions, this.differences],
    [decomposeActions, this.differences, this.todo]
  ], cb)
}

function move (node, hoistTo, diff) {
  node.parent.children = without(node.parent.children, node)
  hoistTo.children.push(node)
  node.fromPath = node.path
  node.path = childPath(hoistTo.path, node)
  node.parent = hoistTo
  if (!diff.filter(function (action) { return action[0] === 'move' && action[1] === node }).length) {
    diff.push(['move', node])
  }
}

function moveRemainingChildren (node, diff) {
  node.children.forEach(function (child) {
    move(child, node, diff)
    moveRemainingChildren(child, diff)
  })
}

function remove (child, diff, done) {
  remove_(child, diff, {}, done)
}

function remove_ (child, diff, seen, done) {
  if (seen[child.path]) return done()
  seen[child.path] = true
  diff.push(['remove', child])
  child.parent.children = without(child.parent.children, child)
  asyncMap(child.children, function (child, next) {
    remove_(child, diff, seen, next)
  }, done)
}

function hoistChildren (tree, diff, next) {
  hoistChildren_(tree, diff, {}, next)
}

function hoistChildren_ (tree, diff, seen, next) {
  validate('OAOF', arguments)
  if (seen[tree.path]) return next()
  seen[tree.path] = true
  asyncMap(tree.children, function (child, done) {
    if (!tree.parent) return hoistChildren_(child, diff, seen, done)
    var better = findRequirement(tree.parent, moduleName(child), getRequested(child) || npa(packageId(child)))
    if (better) {
      return chain([
        [remove, child, diff],
        [andComputeMetadata(tree)]
      ], done)
    }
    var hoistTo = earliestInstallable(tree, tree.parent, child.package)
    if (hoistTo) {
      move(child, hoistTo, diff)
      chain([
        [andComputeMetadata(hoistTo)],
        [hoistChildren_, child, diff, seen],
        [ function (next) {
          moveRemainingChildren(child, diff)
          next()
        } ]
      ], done)
    } else {
      done()
    }
  }, next)
}
