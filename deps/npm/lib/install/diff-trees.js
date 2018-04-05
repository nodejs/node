'use strict'
var npm = require('../npm.js')
var validate = require('aproba')
var npa = require('npm-package-arg')
var flattenTree = require('./flatten-tree.js')
var isOnlyDev = require('./is-only-dev.js')
var log = require('npmlog')
var path = require('path')
var ssri = require('ssri')
var moduleName = require('../utils/module-name.js')

// we don't use get-requested because we're operating on files on disk, and
// we don't want to extropolate from what _should_ be there.
function pkgRequested (pkg) {
  return pkg._requested || (pkg._resolved && npa(pkg._resolved)) || (pkg._from && npa(pkg._from))
}

function nonRegistrySource (requested) {
  if (fromGit(requested)) return true
  if (fromLocal(requested)) return true
  if (fromRemote(requested)) return true
  return false
}

function fromRemote (requested) {
  if (requested.type === 'remote') return true
}

function fromLocal (requested) {
  // local is an npm@3 type that meant "file"
  if (requested.type === 'file' || requested.type === 'directory' || requested.type === 'local') return true
  return false
}

function fromGit (requested) {
  if (requested.type === 'hosted' || requested.type === 'git') return true
  return false
}

function pkgIntegrity (pkg) {
  try {
    // dist is provided by the registry
    var sri = (pkg.dist && pkg.dist.integrity) ||
              // _integrity is provided by pacote
              pkg._integrity ||
              // _shasum is legacy
              (pkg._shasum && ssri.fromHex(pkg._shasum, 'sha1').toString())
    if (!sri) return
    var integrity = ssri.parse(sri)
    if (Object.keys(integrity).length === 0) return
    return integrity
  } catch (ex) {
    return
  }
}

function sriMatch (aa, bb) {
  if (!aa || !bb) return false
  for (let algo of Object.keys(aa)) {
    if (!bb[algo]) continue
    for (let aaHash of aa[algo]) {
      for (let bbHash of bb[algo]) {
        return aaHash.digest === bbHash.digest
      }
    }
  }
  return false
}

function pkgAreEquiv (aa, bb) {
  // coming in we know they share a path…

  // if they share package metadata _identity_, they're the same thing
  if (aa.package === bb.package) return true
  // if they share integrity information, they're the same thing
  var aaIntegrity = pkgIntegrity(aa.package)
  var bbIntegrity = pkgIntegrity(bb.package)
  if (aaIntegrity || bbIntegrity) return sriMatch(aaIntegrity, bbIntegrity)

  // if they're links and they share the same target, they're the same thing
  if (aa.isLink && bb.isLink) return aa.realpath === bb.realpath

  // if we can't determine both their sources then we have no way to know
  // if they're the same thing, so we have to assume they aren't
  var aaReq = pkgRequested(aa.package)
  var bbReq = pkgRequested(bb.package)
  if (!aaReq || !bbReq) return false

  if (fromGit(aaReq) && fromGit(bbReq)) {
    // if both are git and share a _resolved specifier (one with the
    // comittish replaced by a commit hash) then they're the same
    return aa.package._resolved && bb.package._resolved &&
           aa.package._resolved === bb.package._resolved
  }

  // we have to give up trying to find matches for non-registry sources at this point…
  if (nonRegistrySource(aaReq) || nonRegistrySource(bbReq)) return false

  // finally, if they ARE a registry source then version matching counts
  return aa.package.version === bb.package.version
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
  var toRemoveByName = {}

  // Build our tentative remove list.  We don't add remove actions yet
  // because we might resuse them as part of a move.
  Object.keys(flatOldTree).forEach(function (flatname) {
    if (flatname === '/') return
    if (flatNewTree[flatname]) return
    var pkg = flatOldTree[flatname]
    if (pkg.isInLink && /^[.][.][/\\]/.test(path.relative(newTree.realpath, pkg.realpath))) return

    toRemove[flatname] = pkg
    var name = moduleName(pkg)
    if (!toRemoveByName[name]) toRemoveByName[name] = []
    toRemoveByName[name].push({flatname: flatname, pkg: pkg})
  })

  // generate our add/update/move actions
  Object.keys(flatNewTree).forEach(function (flatname) {
    if (flatname === '/') return
    var pkg = flatNewTree[flatname]
    var oldPkg = pkg.oldPkg = flatOldTree[flatname]
    if (oldPkg) {
      // if the versions are equivalent then we don't need to update… unless
      // the user explicitly asked us to.
      if (!pkg.userRequired && pkgAreEquiv(oldPkg, pkg)) return
      setAction(differences, 'update', pkg)
    } else {
      var name = moduleName(pkg)
      // find any packages we're removing that share the same name and are equivalent
      var removing = (toRemoveByName[name] || []).filter((rm) => pkgAreEquiv(rm.pkg, pkg))
      var bundlesOrFromBundle = pkg.fromBundle || pkg.package.bundleDependencies
      // if we have any removes that match AND we're not working with a bundle then upgrade to a move
      if (removing.length && !bundlesOrFromBundle) {
        var toMv = removing.shift()
        toRemoveByName[name] = toRemoveByName[name].filter((rm) => rm !== toMv)
        pkg.fromPath = toMv.pkg.path
        setAction(differences, 'move', pkg)
        delete toRemove[toMv.flatname]
      // we don't generate add actions for things found in links (which already exist on disk) or
      // for bundled modules (which will be installed when we install their parent)
      } else if (!(pkg.isInLink && pkg.fromBundle)) {
        setAction(differences, 'add', pkg)
      }
    }
  })

  // finally generate our remove actions from any not consumed by moves
  Object
    .keys(toRemove)
    .map((flatname) => toRemove[flatname])
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
