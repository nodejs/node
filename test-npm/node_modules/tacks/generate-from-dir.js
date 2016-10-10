'use strict'
var loadFromDir = require('./load-from-dir.js')

module.exports = function generateFromDir (dir) {
  return "var Tacks = require('tacks')\n" +
      'var File = Tacks.File\n' +
      'var Symlink = Tacks.Symlink\n' +
      'var Dir = Tacks.Dir\n' +
      'module.exports = ' + loadFromDir(dir).toSource() + '\n'
}
