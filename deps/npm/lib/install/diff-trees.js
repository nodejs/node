'use strict'
var npm = require('../npm.js')
var validate = require('aproba')
var npa = require('npm-package-arg')
var flattenTree = require('./flatten-tree.js')
var isOnlyDev = require('./is-only-dev.js')
var log = require('npmlog')
var path = require('path')

function nonRegistrySource (pkg) {
  validate('O', arguments)
  var requested = pkg._requested || (pkg._from && npa(pkg._from))
  if (!requested) return false

  if (requested.type === 'hosted') return true
  if (requested.type === 'file' || requested.type === 'directory') return true
  return false
}

function pkgAreEquiv (aa, bb) {
  var aaSha = (aa.dist && aa.dist.integrity) || aa._integrity
  var bbSha = (bb.dist && bb.dist.integrity) || bb._integrity
  if (aaSha === bbSha) return true
  if (aaSha || bbSha) return false
  if (nonRegistrySource(aa) || nonRegistrySource(bb)) return false
  if (aa.version === bb.version) return true
  return false
}

function getUniqueId (pkg) {
  var versionspec = pkg._integrity

  if (!versionspec && nonRegistrySource(pkg)) {
    if (pkg._requested) {
      versionspec = pkg._requested.fetchSpec
    } else if (pkg._from) {
      versionspec = npa(pkg._from).fetchSpec
    }
  }
  if (!versionspec) {
    versionspec = pkg.version
  }
  return pkg.name + '@' + versionspec
}

function pushAll (aa, bb) {
  Array.prototype.push.apply(aa, bb)
}

module.exports = function (oldTree, newTree, differences, log, next) {
  validate('OOAOF', arguments)
  pushAll(differences, sortActions(diffTrees(oldTree, newTree)))
  log.finish()
  next()
}

function isNotTopOrExtraneous (node) {
  return !node.isTop && !node.userRequired && !node.existing
}

var sortActions = module.exports.sortActions = function (differences) {
  var actions = {}
  differences.forEach(function (action) {
    var child = action[1]
    actions[child.location] = action
  })

  var sorted = []
  var added = {}

  var sortedlocs = Object.keys(actions).sort(sortByLocation)

  // We're going to sort the actions taken on top level dependencies first, before
  // considering the order of transitive deps. Because we're building our list
  // from the bottom up, this means we will return a list with top level deps LAST.
  // This is important in terms of keeping installations as consistent as possible
  // as folks add new dependencies.
  var toplocs = sortedlocs.filter(function (location) {
    var mod = actions[location][1]
    if (!mod.requiredBy) return true
    // If this module is required by any non-top level module
    // or by any extraneous module, eg user requested or existing
    // then we don't want to give this priority sorting.
    return !mod.requiredBy.some(isNotTopOrExtraneous)
  })

  toplocs.concat(sortedlocs).forEach(function (location) {
    sortByDeps(actions[location])
  })

  function sortByLocation (aa, bb) {
    return bb.localeCompare(aa)
  }
  function sortModuleByLocation (aa, bb) {
    return sortByLocation(aa && aa.location, bb && bb.location)
  }
  function sortByDeps (action) {
    var mod = action[1]
    if (added[mod.location]) return
    added[mod.location] = action
    if (!mod.requiredBy) mod.requiredBy = []
    mod.requiredBy.sort(sortModuleByLocation).forEach(function (mod) {
      if (actions[mod.location]) sortByDeps(actions[mod.location])
    })
    sorted.unshift(action)
  }

  return sorted
}

function setAction (differences, action, pkg) {
  differences.push([action, pkg])
}

var diffTrees = module.exports._diffTrees = function (oldTree, newTree) {
  validate('OO', arguments)
  var differences = []
  var flatOldTree = flattenTree(oldTree)
  var flatNewTree = flattenTree(newTree)
  var toRemove = {}
  var toRemoveByUniqueId = {}
  // find differences
  Object.keys(flatOldTree).forEach(function (flatname) {
    if (flatNewTree[flatname]) return
    var pkg = flatOldTree[flatname]
    if (pkg.isInLink && /^[.][.][/\\]/.test(path.relative(newTree.realpath, pkg.realpath))) return

    toRemove[flatname] = pkg
    var pkgunique = getUniqueId(pkg.package)
    if (!toRemoveByUniqueId[pkgunique]) toRemoveByUniqueId[pkgunique] = []
    toRemoveByUniqueId[pkgunique].push(flatname)
  })
  Object.keys(flatNewTree).forEach(function (path) {
    var pkg = flatNewTree[path]
    pkg.oldPkg = flatOldTree[path]
    if (pkg.oldPkg) {
      if (!pkg.userRequired && pkgAreEquiv(pkg.oldPkg.package, pkg.package)) return
      setAction(differences, 'update', pkg)
    } else {
      var vername = getUniqueId(pkg.package)
      var removing = toRemoveByUniqueId[vername] && toRemoveByUniqueId[vername].length
      var bundlesOrFromBundle = pkg.fromBundle || pkg.package.bundleDependencies
      if (removing && !bundlesOrFromBundle) {
        var flatname = toRemoveByUniqueId[vername].shift()
        pkg.fromPath = toRemove[flatname].path
        setAction(differences, 'move', pkg)
        delete toRemove[flatname]
      } else if (!(pkg.isInLink && pkg.fromBundle)) {
        setAction(differences, 'add', pkg)
      }
    }
  })
  Object
    .keys(toRemove)
    .map((path) => toRemove[path])
    .forEach((pkg) => setAction(differences, 'remove', pkg))

  const includeDev = npm.config.get('dev') ||
    (!/^prod(uction)?$/.test(npm.config.get('only')) && !npm.config.get('production')) ||
    /^dev(elopment)?$/.test(npm.config.get('only')) ||
    /^dev(elopment)?$/.test(npm.config.get('also'))
  const includeProd = !/^dev(elopment)?$/.test(npm.config.get('only'))
  if (!includeProd || !includeDev) {
    log.silly('diff-trees', 'filtering actions:', 'includeDev', includeDev, 'includeProd', includeProd)
    differences = differences.filter((diff) => {
      const pkg = diff[1]
      const pkgIsOnlyDev = isOnlyDev(pkg)
      return (!includeProd && pkgIsOnlyDev) || (includeDev && pkgIsOnlyDev) || (includeProd && !pkgIsOnlyDev)
    })
  }
  return differences
}
