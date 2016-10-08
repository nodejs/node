'use strict'
var path = require('path')
var mkdirp = require('mkdirp')
var inherits = require('util').inherits
var Entry = require('./entry')
var asObjectKey = require('./as-object-key.js')

module.exports = Dir

function Dir (contents) {
  if (this == null) return new Dir(contents)
  Entry.call(this, 'dir', contents || {})
}
inherits(Dir, Entry)

Dir.prototype.forContents = function (cb) {
  var contentNames = Object.keys(this.contents)
  for (var ii in contentNames) {
    var name = contentNames[ii]
    cb.call(this, this.contents[name], name, ii, contentNames)
  }
}

Dir.prototype.computePath = function (entitypath) {
  Entry.prototype.computePath.call(this, entitypath)

  this.forContents(function (content, name) {
    content.computePath(path.join(entitypath, name))
  })
}

Dir.prototype.create = function (where) {
  var subdirpath = path.resolve(where, this.path)
  mkdirp.sync(subdirpath)

  this.forContents(function (content) {
    content.create(where)
  })
}

Dir.prototype.remove = function (where) {
  this.forContents(function (content) {
    content.remove(where)
  })

  Entry.prototype.remove.call(this, where)
}

Dir.prototype.toSource = function () {
  var output = 'Dir({\n'
  this.forContents(function (content, filename, ii, keys) {
    var key = asObjectKey(filename)
    var value = content.toSource()
    output += '  ' + key + ': ' + value.replace(/(\n)(.)/mg, '$1  $2')
    if (ii < keys.length - 1) output += ','
    output += '\n'
  })
  return output + '})'
}
