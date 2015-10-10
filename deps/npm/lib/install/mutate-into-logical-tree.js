'use strict'
var union = require('lodash.union')
var without = require('lodash.without')
var validate = require('aproba')
var flattenTree = require('./flatten-tree.js')
var isExtraneous = require('./is-extraneous.js')
var validateAllPeerDeps = require('./deps.js').validateAllPeerDeps
var getPackageId = require('./get-package-id.js')

var mutateIntoLogicalTree = module.exports = function (tree) {
  validate('O', arguments)

  validateAllPeerDeps(tree, function (tree, pkgname, version) {
    if (!tree.missingPeers) tree.missingPeers = {}
    tree.missingPeers[pkgname] = version
  })

  var flat = flattenTree(tree)

  function getNode (flatname) {
    return flatname.substr(0, 5) === '#DEV:' ?
           flat[flatname.substr(5)] :
           flat[flatname]
  }

  Object.keys(flat).sort().forEach(function (flatname) {
    var node = flat[flatname]
    var requiredBy = node.package._requiredBy || []
    var requiredByNames = requiredBy.filter(function (parentFlatname) {
      var parentNode = getNode(parentFlatname)
      if (!parentNode) return false
      return parentNode.package.dependencies[node.package.name] ||
             (parentNode.package.devDependencies && parentNode.package.devDependencies[node.package.name])
    })
    requiredBy = requiredByNames.map(getNode)

    node.requiredBy = requiredBy

    if (!requiredBy.length) return

    if (node.parent) node.parent.children = without(node.parent.children, node)

    requiredBy.forEach(function (parentNode) {
      parentNode.children = union(parentNode.children, [node])
    })
    if (node.package._requiredBy.some(function (nodename) { return nodename[0] === '#' })) {
      tree.children = union(tree.children, [node])
    }
  })
  return tree
}

module.exports.asReadInstalled = function (tree) {
  mutateIntoLogicalTree(tree)
  return translateTree(tree)
}

function translateTree (tree) {
  return translateTree_(tree, {})
}

function translateTree_ (tree, seen) {
  var pkg = tree.package
  if (seen[tree.path]) return pkg
  seen[tree.path] = pkg
  if (pkg._dependencies) return pkg
  pkg._dependencies = pkg.dependencies
  pkg.dependencies = {}
  tree.children.forEach(function (child) {
    pkg.dependencies[child.package.name] = translateTree_(child, seen)
  })
  Object.keys(tree.missingDeps).forEach(function (name) {
    if (pkg.dependencies[name]) {
      pkg.dependencies[name].invalid = true
      pkg.dependencies[name].realName = name
      pkg.dependencies[name].extraneous = false
    } else {
      pkg.dependencies[name] = {
        requiredBy: tree.missingDeps[name],
        missing: true,
        optional: !!pkg.optionalDependencies[name]
      }
    }
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
        requiredBy: getPackageId(child),
        requires: pkgname + '@' + version
      })
    })
  })
  pkg.path = tree.path

  pkg.error = tree.error
  pkg.extraneous = isExtraneous(tree)
  if (tree.target && tree.parent && !tree.parent.target) pkg.link = tree.realpath
  return pkg
}
