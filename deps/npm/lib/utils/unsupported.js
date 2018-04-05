'use strict'
var semver = require('semver')
var supportedNode = [
  {ver: '4', min: '4.7.0'},
  {ver: '6', min: '6.0.0'},
  {ver: '7', min: '7.0.0'},
  {ver: '8', min: '8.0.0'},
  {ver: '9', min: '9.0.0'},
  {ver: '10', min: '10.0.0'}
]
var knownBroken = '<4.7.0'

var checkVersion = exports.checkVersion = function (version) {
  var versionNoPrerelease = version.replace(/-.*$/, '')
  return {
    version: versionNoPrerelease,
    broken: semver.satisfies(versionNoPrerelease, knownBroken),
    unsupported: !semver.satisfies(versionNoPrerelease, supportedNode.map(function (n) { return '^' + n.min }).join('||'))
  }
}

exports.checkForBrokenNode = function () {
  var nodejs = checkVersion(process.version)
  if (nodejs.broken) {
    console.error('ERROR: npm is known not to run on Node.js ' + process.version)
    supportedNode.forEach(function (rel) {
      if (semver.satisfies(nodejs.version, rel.ver)) {
        console.error('Node.js ' + rel.ver + " is supported but the specific version you're running has")
        console.error(`a bug known to break npm. Please update to at least ${rel.min} to use this`)
        console.error('version of npm. You can find the latest release of Node.js at https://nodejs.org/')
        process.exit(1)
      }
    })
    var supportedMajors = supportedNode.map(function (n) { return n.ver }).join(', ')
    console.error("You'll need to upgrade to a newer version in order to use this")
    console.error('version of npm. Supported versions are ' + supportedMajors + '. You can find the')
    console.error('latest version at https://nodejs.org/')
    process.exit(1)
  }
}

exports.checkForUnsupportedNode = function () {
  var nodejs = checkVersion(process.version)
  if (nodejs.unsupported) {
    var log = require('npmlog')
    var supportedMajors = supportedNode.map(function (n) { return n.ver }).join(', ')
    log.warn('npm', 'npm does not support Node.js ' + process.version)
    log.warn('npm', 'You should probably upgrade to a newer version of node as we')
    log.warn('npm', "can't make any promises that npm will work with this version.")
    log.warn('npm', 'Supported releases of Node.js are the latest release of ' + supportedMajors + '.')
    log.warn('npm', 'You can find the latest version at https://nodejs.org/')
  }
}
