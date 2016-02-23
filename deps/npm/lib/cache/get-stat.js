var npm = require('../npm.js')
var correctMkdir = require('../utils/correct-mkdir.js')

module.exports = function getCacheStat (cb) {
  correctMkdir(npm.cache, cb)
}
