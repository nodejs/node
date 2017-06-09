'use strict'
var path = require('path')
var writeFileAtomic = require('write-file-atomic')
var moduleName = require('../utils/module-name.js')
var deepSortObject = require('../utils/deep-sort-object.js')
var sortedObject = require('sorted-object')

var sortKeys = [
  'dependencies', 'devDependencies', 'bundleDependencies',
  'optionalDependencies', 'keywords', 'engines', 'scripts',
  'files'
]

module.exports = function (mod, buildpath, next) {
  var pkg = sortedObject(mod.package)
  var name = moduleName(mod)
  // Add our diagnostic keys to the package.json.
  // Note that there are folks relying on these, for ex, the Visual Studio
  // Node.js addon.
  pkg._requiredBy =
    mod.requiredBy
      .map(function (req) {
        if (req.package.devDependencies[name] && !req.package.dependencies[name]) {
          return '#DEV:' + req.location
        } else {
          return req.location
        }
      })
      .concat(mod.userRequired ? ['#USER'] : [])
      .concat(mod.existing ? ['#EXISTING'] : [])
      .sort()
  pkg._location = mod.location
  pkg._phantomChildren = {}
  Object.keys(mod.phantomChildren).sort().forEach(function (name) {
    pkg._phantomChildren[name] = mod.phantomChildren[name].package.version
  })

  // sort keys that are known safe to sort to produce more consistent output
  sortKeys.forEach(function (key) {
    if (pkg[key] != null) pkg[key] = deepSortObject(pkg[key])
  })

  var data = JSON.stringify(sortedObject(pkg), null, 2) + '\n'

  writeFileAtomic(path.resolve(buildpath, 'package.json'), data, next)
}
