'use strict'
var path = require('path')
var validate = require('aproba')

module.exports = function inflateBundled (parent, children) {
  validate('OA', arguments)
  children.forEach(function (child) {
    child.fromBundle = true
    child.parent = parent
    child.path = path.join(parent.path, child.package.name)
    child.realpath = path.resolve(parent.realpath, child.package.name)
    child.isLink = child.isLink || parent.isLink || parent.target
    inflateBundled(child, child.children)
  })
}
