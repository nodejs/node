const semver = require('semver')
const supported = require('../../package.json').engines.node
const knownBroken = '<6.2.0 || 9 <9.3.0'

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
    const log = require('npmlog')
    log.warn('npm', 'npm does not support Node.js ' + process.version)
    log.warn('npm', 'You should probably upgrade to a newer version of node as we')
    log.warn('npm', "can't make any promises that npm will work with this version.")
    log.warn('npm', 'You can find the latest version at https://nodejs.org/')
  }
}
