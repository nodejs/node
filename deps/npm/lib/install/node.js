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
function isInLink (node) {
  return node && (node.isInLink || node.isLink)
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
    if (node.isLink == null) node.isLink = isLink(node.parent)
    if (node.isInLink == null) node.isInLink = isInLink(node.parent)
    if (node.fromBundle == null) {
      node.fromBundle = false
    }
  }
  return node
}

exports.reset = function (node) {
  reset(node, new Set())
}

function reset (node, seen) {
  if (seen.has(node)) return
  seen.add(node)
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
