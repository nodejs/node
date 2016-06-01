'use strict'
var url = require('url')
var asyncMap = require('slide').asyncMap
var validate = require('aproba')
var iferr = require('iferr')
var realizePackageSpecifier = require('realize-package-specifier')
var fetchPackageMetadata = require('../fetch-package-metadata.js')
var annotateMetadata = require('../fetch-package-metadata.js').annotateMetadata
var addShrinkwrap = require('../fetch-package-metadata.js').addShrinkwrap
var addBundled = require('../fetch-package-metadata.js').addBundled
var inflateBundled = require('./inflate-bundled.js')
var npm = require('../npm.js')
var createChild = require('./node.js').create
var moduleName = require('../utils/module-name.js')
var childPath = require('../utils/child-path.js')

var inflateShrinkwrap = module.exports = function (tree, swdeps, finishInflating) {
  validate('OOF', arguments)
  if (!npm.config.get('shrinkwrap')) return finishInflating()
  var onDisk = {}
  tree.children.forEach(function (child) { onDisk[moduleName(child)] = child })
  tree.children = []
  asyncMap(Object.keys(swdeps), function (name, next) {
    var sw = swdeps[name]
    var spec = sw.resolved
             ? name + '@' + sw.resolved
             : (sw.from && url.parse(sw.from).protocol)
             ? name + '@' + sw.from
             : name + '@' + sw.version
    var child = onDisk[name]
    if (child && (child.fromShrinkwrap ||
                  (sw.resolved && child.package._resolved === sw.resolved) ||
                  (sw.from && url.parse(sw.from).protocol && child.package._from === sw.from) ||
                  child.package.version === sw.version)) {
      if (!child.fromShrinkwrap) child.fromShrinkwrap = spec
      tree.children.push(child)

      return realizePackageSpecifier(spec, tree.path, iferr(next, function (requested) {
        annotateMetadata(child.package, requested, spec, tree.path)
        return inflateShrinkwrap(child, sw.dependencies || {}, next)
      }))
    }
    fetchPackageMetadata(spec, tree.path, iferr(next, function (pkg) {
      pkg._from = sw.from || spec
      addShrinkwrap(pkg, iferr(next, function () {
        addBundled(pkg, iferr(next, function () {
          var child = createChild({
            package: pkg,
            loaded: false,
            parent: tree,
            fromShrinkwrap: spec,
            path: childPath(tree.path, pkg),
            realpath: childPath(tree.realpath, pkg),
            children: pkg._bundled || []
          })
          tree.children.push(child)
          if (pkg._bundled) {
            inflateBundled(child, child.children)
          }
          inflateShrinkwrap(child, sw.dependencies || {}, next)
        }))
      }))
    }))
  }, finishInflating)
}
