const { spawn } = require('@npmcli/git')
const semver = require('semver')

module.exports = async opts => {
  const tag = (await spawn(['describe', '--tags', '--abbrev=0', '--match=*.*.*'], opts)).stdout.trim()
  const ver = semver.coerce(tag, { loose: true })
  if (ver) {
    return ver.version
  }
  throw new Error(`Tag is not a valid version: ${JSON.stringify(tag)}`)
}
