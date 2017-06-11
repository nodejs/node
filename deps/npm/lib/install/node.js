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
  save: false,
  saveSpec: null,
  isTop: false,
  fromBundle: false
}

function isLink (node) {
  return node && node.isLink
}

var create = exports.create = function (node, template, isNotTop) {
  if (!template) template = defaultTemplate
  Object.keys(template).forEach(function (key) {
    if (template[key] != null && typeof template[key] === 'object' && !(template[key] instanceof Array)) {
      if (!node[key]) node[key] = {}
      return create(node[key], template[key], true)
    }
    if (node[key] != null) return
    node[key] = template[key]
  })
  if (!isNotTop) {
    // isLink is true for the symlink and everything inside it.
    // by contrast, isInLink is true for only the things inside a link
    if (node.isLink == null && isLink(node.parent)) {
      node.isLink = true
      node.isInLink = true
    } else if (node.isLink == null) {
      node.isLink = false
      node.isInLink = false
    }
    if (node.fromBundle == null && node.package) {
      node.fromBundle = node.package._inBundle
    } else if (node.fromBundle == null) {
      node.fromBundle = false
    }
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
