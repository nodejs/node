var semver = require('semver')
var version = semver.parse(require('../package.json').version)
console.log('next-%s', version.major)
