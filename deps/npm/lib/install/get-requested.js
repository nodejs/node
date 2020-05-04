'use strict'
const npa = require('npm-package-arg')
const moduleName = require('../utils/module-name.js')
const packageRelativePath = require('./deps').packageRelativePath
module.exports = function (child, reqBy) {
  if (!child.requiredBy.length) return
  if (!reqBy) reqBy = child.requiredBy[0]
  const deps = reqBy.package.dependencies || {}
  const devDeps = reqBy.package.devDependencies || {}
  const optDeps = reqBy.package.optionalDependencies || {}
  const name = moduleName(child)
  const spec = deps[name] || devDeps[name] || optDeps[name]
  const where = packageRelativePath(reqBy)
  return npa.resolve(name, spec, where)
}
