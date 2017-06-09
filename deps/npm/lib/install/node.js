'use strict'

var defaultTemplate = {
  package: {
    version: '',
    dependencies: {},
    devDependencies: {},
    optionalDependencies: {}
  },
  loaded: false,
  children: [],
  requiredBy: [],
  requires: [],
  missingDeps: {},
  missingDevDeps: {},
  phantomChildren: {},
  path: null,
  realpath: null,
  location: null,
  userRequired: false,
  existing: false,
  isTop: false
}

function isLink (node) {
  return node && node.isLink
}

var create = exports.create = function (node, template) {
  if (!template) template = defaultTemplate
  Object.keys(template).forEach(function (key) {
    if (template[key] != null && typeof template[key] === 'object' && !(template[key] instanceof Array)) {
      if (!node[key]) node[key] = {}
      return create(node[key], template[key])
    }
    if (node[key] != null) return
    node[key] = template[key]
  })
  if (isLink(node.parent)) {
    node.isLink = true
  }
  return node
}

exports.reset = function (node) {
  reset(node, {})
}

function reset (node, seen) {
  if (seen[node.path]) return
  seen[node.path] = true
  var child = create(node)

  // FIXME: cleaning up after read-package-json's mess =(
  if (child.package._id === '@') delete child.package._id

  child.isTop = false
  child.requiredBy = []
  child.requires = []
  child.missingDeps = {}
  child.missingDevDeps = {}
  child.phantomChildren = {}
  child.location = null

  child.children.forEach(function (child) { reset(child, seen) })
}
