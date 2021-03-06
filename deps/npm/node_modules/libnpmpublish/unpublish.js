'use strict'

const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const semver = require('semver')
const { URL } = require('url')

const unpublish = async (spec, opts) => {
  spec = npa(spec)
  // spec is used to pick the appropriate registry/auth combo.
  opts = {
    force: false,
    ...opts,
    spec,
  }

  try {
    const pkgUri = spec.escapedName
    const pkg = await npmFetch.json(pkgUri, {
      ...opts,
      query: { write: true },
    })

    const version = spec.rawSpec
    const allVersions = pkg.versions || {}
    const versionData = allVersions[version]

    const rawSpecs = (!spec.rawSpec || spec.rawSpec === '*')
    const onlyVersion = Object.keys(allVersions).length === 1
    const noVersions = !Object.keys(allVersions).length

    // if missing specific version,
    // assumed unpublished
    if (!versionData && !rawSpecs && !noVersions)
      return true

    // unpublish all versions of a package:
    // - no specs supplied "npm unpublish foo"
    // - all specs ("*") "npm unpublish foo@*"
    // - there was only one version
    // - has no versions field on packument
    if (rawSpecs || onlyVersion || noVersions) {
      await npmFetch(`${pkgUri}/-rev/${pkg._rev}`, {
        ...opts,
        method: 'DELETE',
        ignoreBody: true,
      })
      return true
    } else {
      const dist = allVersions[version].dist
      delete allVersions[version]

      const latestVer = pkg['dist-tags'].latest

      // deleting dist tags associated to version
      Object.keys(pkg['dist-tags']).forEach(tag => {
        if (pkg['dist-tags'][tag] === version)
          delete pkg['dist-tags'][tag]
      })

      if (latestVer === version) {
        pkg['dist-tags'].latest = Object.keys(
          allVersions
        ).sort(semver.compareLoose).pop()
      }

      delete pkg._revisions
      delete pkg._attachments

      // Update packument with removed versions
      await npmFetch(`${pkgUri}/-rev/${pkg._rev}`, {
        ...opts,
        method: 'PUT',
        body: pkg,
        ignoreBody: true,
      })

      // Remove the tarball itself
      const { _rev } = await npmFetch.json(pkgUri, {
        ...opts,
        query: { write: true },
      })
      const tarballUrl = new URL(dist.tarball).pathname.substr(1)
      await npmFetch(`${tarballUrl}/-rev/${_rev}`, {
        ...opts,
        method: 'DELETE',
        ignoreBody: true,
      })
      return true
    }
  } catch (err) {
    if (err.code !== 'E404')
      throw err

    return true
  }
}

module.exports = unpublish
