'use strict'
module.exports = hasModernMeta

const npa = require('npm-package-arg')
const moduleName = require('../utils/module-name.js')

function isLink (child) {
  return child.isLink || (child.parent && isLink(child.parent))
}

function hasModernMeta (child) {
  if (!child) return false
  const resolved = child.package._resolved && npa.resolve(moduleName(child), child.package._resolved)
  const version = npa.resolve(moduleName(child), child.package.version)
  return child.isTop ||
    isLink(child) ||
    child.fromBundle || child.package._inBundle ||
    child.package._integrity || child.package._shasum ||
    (resolved && resolved.type === 'git') || (version && version.type === 'git')
}
