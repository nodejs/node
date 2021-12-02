/* eslint-disable no-console */
const semver = require('semver')
const supported = require('../../package.json').engines.node
const knownBroken = '<6.2.0 || 9 <9.3.0'

// Keep this file compatible with all practical versions of node
// so we dont get syntax errors when trying to give the users
// a nice error message. Don't use our log handler because
// if we encounter a syntax error early on, that will never
// get displayed to the user.

const checkVersion = exports.checkVersion = version => {
  const versionNoPrerelease = version.replace(/-.*$/, '')
  return {
    version: versionNoPrerelease,
    broken: semver.satisfies(versionNoPrerelease, knownBroken),
    unsupported: !semver.satisfies(versionNoPrerelease, supported),
  }
}

exports.checkForBrokenNode = () => {
  const nodejs = checkVersion(process.version)
  if (nodejs.broken) {
    console.error('ERROR: npm is known not to run on Node.js ' + process.version)
    console.error("You'll need to upgrade to a newer Node.js version in order to use this")
    console.error('version of npm. You can find the latest version at https://nodejs.org/')
    process.exit(1)
  }
}

exports.checkForUnsupportedNode = () => {
  const nodejs = checkVersion(process.version)
  if (nodejs.unsupported) {
    console.error('npm does not support Node.js ' + process.version)
    console.error('You should probably upgrade to a newer version of node as we')
    console.error("can't make any promises that npm will work with this version.")
    console.error('You can find the latest version at https://nodejs.org/')
  }
}
