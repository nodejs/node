'use strict'

const figgyPudding = require('figgy-pudding')
const npa = require('npm-package-arg')
const semver = require('semver')

const PickerOpts = figgyPudding({
  defaultTag: { default: 'latest' },
  enjoyBy: {},
  includeDeprecated: { default: false }
})

module.exports = pickManifest
function pickManifest (packument, wanted, opts) {
  opts = PickerOpts(opts)
  const time = opts.enjoyBy && packument.time && +(new Date(opts.enjoyBy))
  const spec = npa.resolve(packument.name, wanted)
  const type = spec.type
  if (type === 'version' || type === 'range') {
    wanted = semver.clean(wanted, true) || wanted
  }
  const distTags = packument['dist-tags'] || {}
  const versions = Object.keys(packument.versions || {}).filter(v => {
    return semver.valid(v, true)
  })
  const policyRestrictions = packument.policyRestrictions
  const restrictedVersions = policyRestrictions
    ? Object.keys(policyRestrictions.versions) : []

  function enjoyableBy (v) {
    return !time || (
      packument.time[v] && time >= +(new Date(packument.time[v]))
    )
  }

  let err

  if (!versions.length && !restrictedVersions.length) {
    err = new Error(`No valid versions available for ${packument.name}`)
    err.code = 'ENOVERSIONS'
    err.name = packument.name
    err.type = type
    err.wanted = wanted
    throw err
  }

  let target

  if (type === 'tag' && enjoyableBy(distTags[wanted])) {
    target = distTags[wanted]
  } else if (type === 'version') {
    target = wanted
  } else if (type !== 'range' && enjoyableBy(distTags[wanted])) {
    throw new Error('Only tag, version, and range are supported')
  }

  const tagVersion = distTags[opts.defaultTag]

  if (
    !target &&
    tagVersion &&
    packument.versions[tagVersion] &&
    enjoyableBy(tagVersion) &&
    semver.satisfies(tagVersion, wanted, true)
  ) {
    target = tagVersion
  }

  if (!target && !opts.includeDeprecated) {
    const undeprecated = versions.filter(v => !packument.versions[v].deprecated && enjoyableBy(v)
    )
    target = semver.maxSatisfying(undeprecated, wanted, true)
  }
  if (!target) {
    const stillFresh = versions.filter(enjoyableBy)
    target = semver.maxSatisfying(stillFresh, wanted, true)
  }

  if (!target && wanted === '*' && enjoyableBy(tagVersion)) {
    // This specific corner is meant for the case where
    // someone is using `*` as a selector, but all versions
    // are pre-releases, which don't match ranges at all.
    target = tagVersion
  }

  if (
    !target &&
    time &&
    type === 'tag' &&
    distTags[wanted] &&
    !enjoyableBy(distTags[wanted])
  ) {
    const stillFresh = versions.filter(v =>
      enjoyableBy(v) && semver.lte(v, distTags[wanted], true)
    ).sort(semver.rcompare)
    target = stillFresh[0]
  }

  if (!target && restrictedVersions) {
    target = semver.maxSatisfying(restrictedVersions, wanted, true)
  }

  const manifest = (
    target &&
    packument.versions[target]
  )
  if (!manifest) {
    // Check if target is forbidden
    const isForbidden = target && policyRestrictions && policyRestrictions.versions[target]
    const pckg = `${packument.name}@${wanted}${
      opts.enjoyBy
        ? ` with an Enjoy By date of ${
          new Date(opts.enjoyBy).toLocaleString()
        }. Maybe try a different date?`
        : ''
    }`

    if (isForbidden) {
      err = new Error(`Could not download ${pckg} due to policy violations.\n${policyRestrictions.message}\n`)
      err.code = 'E403'
    } else {
      err = new Error(`No matching version found for ${pckg}.`)
      err.code = 'ETARGET'
    }

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
