'use strict'
var semver = require('semver')
var supportedNode = '0.12 || >= 4'
var knownBroken = '>=0.1 <=0.7'

var checkVersion = exports.checkVersion = function (version) {
  var versionNoPrerelease = version.replace(/-.*$/, '')
  return {
    broken: semver.satisfies(versionNoPrerelease, knownBroken),
    unsupported: !semver.satisfies(versionNoPrerelease, supportedNode)
  }
}

exports.checkForBrokenNode = function () {
  var nodejs = checkVersion(process.version)
  if (nodejs.broken) {
    console.error('ERROR: npm is known not to run on Node.js ' + process.version)
    console.error("You'll need to upgrade to a newer version in order to use this")
    console.error('version of npm. You can find the latest version at https://nodejs.org/')
    process.exit(1)
  }
}

exports.checkForUnsupportedNode = function () {
  var nodejs = checkVersion(process.version)
  if (nodejs.unsupported) {
    var log = require('npmlog')
    log.warn('npm', 'npm does not support Node.js ' + process.version)
    log.warn('npm', 'You should probably upgrade to a newer version of node as we')
    log.warn('npm', "can't make any promises that npm will work with this version.")
    log.warn('npm', 'You can find the latest version at https://nodejs.org/')
  }
}
