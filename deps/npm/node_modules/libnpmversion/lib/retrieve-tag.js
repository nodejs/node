const {spawn} = require('@npmcli/git')
const semver = require('semver')

module.exports = async opts => {
  const tag = (await spawn(['describe', '--abbrev=0'], opts)).stdout.trim()
  const match = tag.match(/v?(\d+\.\d+\.\d+(?:[-+].+)?)/)
  const ver = match && semver.clean(match[1], { loose: true })
  if (ver)
    return ver
  throw new Error(`Tag is not a valid version: ${JSON.stringify(tag)}`)
}
