const CACHE_VERSION = require('../package.json').version
const md5hex = require('md5-hex')
const salt = JSON.stringify({
  istanbul: require('istanbul-lib-coverage/package.json').version,
  nyc: CACHE_VERSION
})

function Hash (code, filename) {
  return md5hex([code, filename, salt]) + '_' + CACHE_VERSION
}

Hash.salt = salt

module.exports = Hash
