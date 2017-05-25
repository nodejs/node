'use strict'
var validate = require('aproba')
var childPath = require('../utils/child-path.js')
var reset = require('./node.js').reset

module.exports = function inflateBundled (bundler, parent, children) {
  validate('OOA', arguments)
  children.forEach(function (child) {
    reset(child)
    child.fromBundle = bundler
    child.isInLink = bundler.isLink
    child.parent = parent
    child.path = childPath(parent.path, child)
    child.realpath = bundler.isLink ? child.realpath : childPath(parent.realpath, child)
    child.isLink = child.isLink || parent.isLink || parent.target
    inflateBundled(bundler, child, child.children)
  })
}
