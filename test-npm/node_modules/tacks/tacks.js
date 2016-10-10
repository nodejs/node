'use strict'
var fs = require('fs')

module.exports = Tacks

Tacks.File = require('./file.js')
Tacks.Dir = require('./dir.js')
Tacks.Symlink = require('./symlink.js')

function Tacks (fixture) {
  this.fixture = fixture
  fixture.computePath('/')
}
Tacks.prototype = {}

Tacks.prototype.create = function (location) {
  this.fixture.create(location)
}

Tacks.prototype.remove = function (location) {
  this.fixture.remove(location)
}

Tacks.prototype.toSource = function () {
  return 'new Tacks(\n' +
    this.fixture.toSource().replace(/(^|\n)/g, '$1  ') + '\n)'
}
