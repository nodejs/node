'use strict'
var path = require('path')
var fs = require('fs')
var inherits = require('util').inherits
var Entry = require('./entry')
var asLiteral = require('./as-literal.js')

module.exports = Symlink

function Symlink (dest) {
  if (this == null) return new Symlink(dest)
  if (dest == null || dest === '') throw new Error('Symlinks must have a destination')
  Entry.call(this, 'symlink', dest)
}
inherits(Symlink, Entry)

Symlink.prototype.create = function (where) {
  var filepath = path.resolve(where, this.path)
  var dest = this.contents
  if (dest[0] === '/') {
    dest = path.resolve(where, dest.slice(1))
  }
  fs.symlinkSync(dest, filepath, 'junction')
}

Symlink.prototype.toSource = function () {
  return 'Symlink(' + asLiteral(this.contents) + ')'
}
