'use strict'
var path = require('path')
var fs = require('graceful-fs')
var Tacks = require('./tacks.js')
var File = Tacks.File
var Dir = Tacks.Dir
var Symlink = Tacks.Symlink

module.exports = function (dir) {
  return new Tacks(loadFromDir(dir))
}

function fromJSON (str) {
  try {
    return JSON.parse(str)
  } catch (ex) {
    return
  }
}

function loadFromDir (dir, top) {
  if (!top) top = dir
  var dirInfo = {}
  fs.readdirSync(dir).forEach(function (filename) {
    if (filename === '.git') return
    var filepath = path.join(dir, filename)
    var fileinfo = fs.lstatSync(filepath)
    if (fileinfo.isSymbolicLink()) {
      var dest = fs.readlinkSync(filepath)
      if (/^(?:[a-z]\:)?[\\/]/i.test(dest)) {
        dest = '/' + path.relative(top, dest)
      }
      dirInfo[filename] = Symlink(dest)
    } else if (fileinfo.isDirectory()) {
      dirInfo[filename] = loadFromDir(filepath, top)
    } else {
      var content = fs.readFileSync(filepath)
      var contentStr = content.toString('utf8')
      var contentJSON = fromJSON(contentStr)
      if (contentJSON !== undefined) {
        dirInfo[filename] = File(contentJSON)
      } else if (/[^\-\w\s~`!@#$%^&*()_=+[\]{}|\\;:'",./<>?]/.test(contentStr)) {
        dirInfo[filename] = File(content)
      } else {
        dirInfo[filename] = File(contentStr)
      }
    }
  })
  return Dir(dirInfo)
}
