'use strict'
var union = require('lodash.union')
var without = require('lodash.without')
var validate = require('aproba')
var flattenTree = require('./flatten-tree.js')
var isExtraneous = require('./is-extraneous.js')
var validateAllPeerDeps = require('./deps.js').validateAllPeerDeps
var packageId = require('../utils/package-id.js')
var moduleName = require('../utils/module-name.js')
var npm = require('../npm.js')

// Return true if tree is a part of a cycle that:
//   A) Never connects to the top of the tree
//   B) Has not not had a point in the cycle arbitraryly declared its top
//      yet.
function isDisconnectedCycle (tree, seen) {
  if (!seen) seen = {}
  if (tree.isTop || tree.cycleTop || tree.requiredBy.length === 0) {
    return false
  } else if (seen[tree.path]) {
    return true
  } else {
    seen[tree.path] = true
    return tree.requiredBy.every(function (node) {
      return isDisconnectedCycle(node, Object.create(seen))
    })
  }
}

var mutateIntoLogicalTree = module.exports = function (tree) {
  validate('O', arguments)

  validateAllPeerDeps(tree, function (tree, pkgname, version) {
    if (!tree.missingPeers) tree.missingPeers = {}
    tree.missingPeers[pkgname] = version
  })

  var flat = flattenTree(tree)

  Object.keys(flat).sort().forEach(function (flatname) {
    var node = flat[flatname]
    if (!(node.requiredBy && node.requiredBy.length)) return

    if (node.parent) {
      // If a node is a cycle that never reaches the root of the logical
      // tree then we'll leave it attached to the root, or else it
      // would go missing. Further we'll note that this is the node in the
      // cycle that we picked arbitrarily to be the one attached to the root.
      // others will fall
      if (isDisconnectedCycle(node)) {
        node.cycleTop = true
      // Nor do we want to disconnect non-cyclical extraneous modules from the tree.
      } else if (node.requiredBy.length) {
        // regular deps though, we do, as we're moving them into the capable
        // hands of the modules that require them.
        node.parent.children = without(node.parent.children, node)
      }
    }

    node.requiredBy.forEach(function (parentNode) {
      parentNode.children = union(parentNode.children, [node])
    })
  })
  return tree
}

module.exports.asReadInstalled = function (tree) {
  mutateIntoLogicalTree(tree)
  return translateTree(tree)
}

function translateTree (tree) {
  return translateTree_(tree, new Set())
}

function translateTree_ (tree, seen) {
  var pkg = tree.package
  if (seen.has(tree)) return pkg
  seen.add(tree)
  if (pkg._dependencies) return pkg
  pkg._dependencies = pkg.dependencies
  pkg.dependencies = {}
  tree.children.forEach(function (child) {
    const dep = pkg.dependencies[moduleName(child)] = translateTree_(child, seen)
    if (child.fakeChild) {
      dep.missing = true
      dep.optional = child.package._optional
      dep.requiredBy = child.package._spec
    }
  })

  function markMissing (name, requiredBy) {
    if (pkg.dependencies[name]) {
      if (pkg.dependencies[name].missing) return
      pkg.dependencies[name].invalid = true
      pkg.dependencies[name].realName = name
      pkg.dependencies[name].extraneous = false
    } else {
      pkg.dependencies[name] = {
        requiredBy: requiredBy,
        missing: true,
        optional: !!pkg.optionalDependencies[name]
      }
    }
  }

  Object.keys(tree.missingDeps).forEach(function (name) {
    markMissing(name, tree.missingDeps[name])
  })
  Object.keys(tree.missingDevDeps).forEach(function (name) {
    markMissing(name, tree.missingDevDeps[name])
  })
  var checkForMissingPeers = (tree.parent ? [] : [tree]).concat(tree.children)
  checkForMissingPeers.filter(function (child) {
    return child.missingPeers
  }).forEach(function (child) {
    Object.keys(child.missingPeers).forEach(function (pkgname) {
      var version = child.missingPeers[pkgname]
      var peerPkg = pkg.dependencies[pkgname]
      if (!peerPkg) {
        peerPkg = pkg.dependencies[pkgname] = {
          _id: pkgname + '@' + version,
          name: pkgname,
          version: version
        }
      }
      if (!peerPkg.peerMissing) peerPkg.peerMissing = []
      peerPkg.peerMissing.push({
        requiredBy: packageId(child),
        requires: pkgname + '@' + version
      })
    })
  })
  pkg.path = tree.path

  pkg.error = tree.error
  pkg.extraneous = !tree.isTop && (!tree.parent.isTop || !tree.parent.error) && !npm.config.get('global') && isExtraneous(tree)
  if (tree.target && tree.parent && !tree.parent.target) pkg.link = tree.realpath
  return pkg
}
