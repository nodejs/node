var assert = require('assert')
var resolve = require('path').resolve

var npm = require('../npm.js')

module.exports = getCacheRoot

function getCacheRoot (data) {
  assert(data, 'must pass package metadata')
  assert(data.name, 'package metadata must include name')
  assert(data.version, 'package metadata must include version')

  return resolve(npm.cache, data.name, data.version)
}
