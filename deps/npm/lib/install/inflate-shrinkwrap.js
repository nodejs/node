'use strict'
var asyncMap = require('slide').asyncMap
var validate = require('aproba')
var iferr = require('iferr')
var realizeShrinkwrapSpecifier = require('./realize-shrinkwrap-specifier.js')
var isRegistrySpecifier = require('./is-registry-specifier.js')
var fetchPackageMetadata = require('../fetch-package-metadata.js')
var annotateMetadata = require('../fetch-package-metadata.js').annotateMetadata
var addShrinkwrap = require('../fetch-package-metadata.js').addShrinkwrap
var addBundled = require('../fetch-package-metadata.js').addBundled
var inflateBundled = require('./inflate-bundled.js')
var npm = require('../npm.js')
var createChild = require('./node.js').create
var moduleName = require('../utils/module-name.js')
var childPath = require('../utils/child-path.js')

module.exports = function (tree, swdeps, finishInflating) {
  if (!npm.config.get('shrinkwrap')) return finishInflating()
  return inflateShrinkwrap(tree.path, tree, swdeps, finishInflating)
}

function inflateShrinkwrap (topPath, tree, swdeps, finishInflating) {
  validate('SOOF', arguments)
  var onDisk = {}
  tree.children.forEach(function (child) { onDisk[moduleName(child)] = child })
  tree.children = []
  var dev = npm.config.get('dev') || (!/^prod(uction)?$/.test(npm.config.get('only')) && !npm.config.get('production')) || /^dev(elopment)?$/.test(npm.config.get('only'))
  var prod = !/^dev(elopment)?$/.test(npm.config.get('only'))
  return asyncMap(Object.keys(swdeps), doRealizeAndInflate, finishInflating)

  function doRealizeAndInflate (name, next) {
    return realizeShrinkwrapSpecifier(name, swdeps[name], topPath, iferr(next, andInflate(name, next)))
  }

  function andInflate (name, next) {
    return function (requested) {
      var sw = swdeps[name]
      var dependencies = sw.dependencies || {}
      if ((!prod && !sw.dev) || (!dev && sw.dev)) return next()
      var child = onDisk[name]
      if (childIsEquivalent(sw, requested, child)) {
        if (!child.fromShrinkwrap) child.fromShrinkwrap = requested.raw
        tree.children.push(child)
        annotateMetadata(child.package, requested, requested.raw, topPath)
        return inflateShrinkwrap(topPath, child, dependencies || {}, next)
      } else {
        var from = sw.from || requested.raw
        var optional = sw.optional
        return fetchPackageMetadata(requested, topPath, iferr(next, andAddShrinkwrap(from, optional, dependencies, next)))
      }
    }
  }

  function andAddShrinkwrap (from, optional, dependencies, next) {
    return function (pkg) {
      pkg._from = from
      pkg._optional = optional
      addShrinkwrap(pkg, iferr(next, andAddBundled(pkg, dependencies, next)))
    }
  }

  function andAddBundled (pkg, dependencies, next) {
    return function () {
      return addBundled(pkg, iferr(next, andAddChild(pkg, dependencies, next)))
    }
  }

  function andAddChild (pkg, dependencies, next) {
    return function () {
      var child = createChild({
        package: pkg,
        loaded: false,
        parent: tree,
        fromShrinkwrap: pkg._from,
        path: childPath(tree.path, pkg),
        realpath: childPath(tree.realpath, pkg),
        children: pkg._bundled || []
      })
      tree.children.push(child)
      if (pkg._bundled) {
        delete pkg._bundled
        inflateBundled(child, child.children)
      }
      inflateShrinkwrap(topPath, child, dependencies || {}, next)
    }
  }
}

function childIsEquivalent (sw, requested, child) {
  if (!child) return false
  if (child.fromShrinkwrap) return true
  if (sw.resolved) return child.package._resolved === sw.resolved
  if (!isRegistrySpecifier(requested) && sw.from) return child.package._from === sw.from
  return child.package.version === sw.version
}
