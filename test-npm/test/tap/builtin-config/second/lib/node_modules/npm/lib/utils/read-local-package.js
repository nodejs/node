exports = module.exports = readLocalPkg

var npm = require('../npm.js')
var readJson = require('read-package-json')

function readLocalPkg (cb) {
  if (npm.config.get('global')) return cb()
  var path = require('path')
  readJson(path.resolve(npm.prefix, 'package.json'), function (er, d) {
    return cb(er, d && d.name)
  })
}
