'use strict'
var path = require('path')
var rimraf = require('rimraf')
module.exports = Entry

function Entry (type, contents) {
  this.type = type
  this.contents = contents
  this.path = null
}
Entry.prototype = {}

Entry.prototype.forContents = function (cb) {
  cb.call(this, this.contents)
}

Entry.prototype.computePath = function (entitypath) {
  this.path = path.relative('/', entitypath)
}

Entry.prototype.create = function (where) {
  throw new Error("Don't know how to create " + this.constructor.name + " at " + where)
}

Entry.prototype.remove = function (where) {
  rimraf.sync(where)
}
