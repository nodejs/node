'use strict'

const npa = require('npm-package-arg')
const semver = require('semver')

module.exports = pickManifest
function pickManifest (packument, wanted, opts) {
  opts = opts || {}
  const spec = npa.resolve(packument.name, wanted)
  const type = spec.type
  if (type === 'version' || type === 'range') {
    wanted = semver.clean(wanted, true) || wanted
  }
  const distTags = packument['dist-tags'] || {}
  const versions = Object.keys(packument.versions || {}).filter(v => semver.valid(v, true))
  let err

  if (!versions.length) {
    err = new Error(`No valid versions available for ${packument.name}`)
    err.code = 'ENOVERSIONS'
    err.name = packument.name
    err.type = type
    err.wanted = wanted
    throw err
  }

  let target

  if (type === 'tag') {
    target = distTags[wanted]
  } else if (type === 'version') {
    target = wanted
  } else if (type !== 'range') {
    throw new Error('Only tag, version, and range are supported')
  }

  const tagVersion = distTags[opts.defaultTag || 'latest']

  if (
    !target &&
    tagVersion &&
    packument.versions[tagVersion] &&
    semver.satisfies(tagVersion, wanted, true)
  ) {
    target = tagVersion
  }

  if (!target) {
    target = semver.maxSatisfying(versions, wanted, true)
  }

  if (!target && wanted === '*') {
    // This specific corner is meant for the case where
    // someone is using `*` as a selector, but all versions
    // are pre-releases, which don't match ranges at all.
    target = tagVersion
  }

  const manifest = target && packument.versions[target]
  if (!manifest) {
    err = new Error(
      `No matching version found for ${packument.name}@${wanted}`
    )
    err.code = 'ETARGET'
    err.name = packument.name
    err.type = type
    err.wanted = wanted
    err.versions = versions
    err.distTags = distTags
    err.defaultTag = opts.defaultTag
    throw err
  } else {
    return manifest
  }
}
