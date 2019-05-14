'use strict'
const npa = require('npm-package-arg')
const moduleName = require('../utils/module-name.js')

module.exports = function (child, reqBy) {
  if (!child.requiredBy.length) return
  if (!reqBy) reqBy = child.requiredBy[0]
  const deps = reqBy.package.dependencies || {}
  const devDeps = reqBy.package.devDependencies || {}
  const name = moduleName(child)
  return npa.resolve(name, deps[name] || devDeps[name], reqBy.realpath)
}
