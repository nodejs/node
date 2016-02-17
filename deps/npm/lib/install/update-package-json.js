'use strict'
var path = require('path')
var writeFileAtomic = require('write-file-atomic')
var deepSortObject = require('../utils/deep-sort-object.js')

module.exports = function (pkg, buildpath, next) {
  // FIXME: This bundled dance is because we're sticking a big tree of bundled
  // deps into the parsed package.json– it probably doesn't belong there =/
  // But the real reason we don't just dump it out is that it's the result
  // of npm-read-tree, which produces circular data structures, due to the
  // parent and children keys.
  var bundled = pkg.package._bundled
  delete pkg.package._bundled // FIXME
  var packagejson = deepSortObject(pkg.package)
  var data = JSON.stringify(packagejson, null, 2) + '\n'
  pkg.package._bundled = bundled
  writeFileAtomic(path.resolve(buildpath, 'package.json'), data, next)
}
