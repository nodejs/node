'use strict'

const figgyPudding = require('figgy-pudding')
const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const semver = require('semver')
const url = require('url')

const UnpublishConfig = figgyPudding({
  force: { default: false },
  Promise: { default: () => Promise }
})

module.exports = unpublish
function unpublish (spec, opts) {
  opts = UnpublishConfig(opts)
  return new opts.Promise(resolve => resolve()).then(() => {
    spec = npa(spec)
    // NOTE: spec is used to pick the appropriate registry/auth combo.
    opts = opts.concat({ spec })
    const pkgUri = spec.escapedName
    return npmFetch.json(pkgUri, opts.concat({
      query: { write: true }
    })).then(pkg => {
      if (!spec.rawSpec || spec.rawSpec === '*') {
        return npmFetch(`${pkgUri}/-rev/${pkg._rev}`, opts.concat({
          method: 'DELETE',
          ignoreBody: true
        }))
      } else {
        const version = spec.rawSpec
        const allVersions = pkg.versions || {}
        const versionPublic = allVersions[version]
        let dist
        if (versionPublic) {
          dist = allVersions[version].dist
        }
        delete allVersions[version]
        // if it was the only version, then delete the whole package.
        if (!Object.keys(allVersions).length) {
          return npmFetch(`${pkgUri}/-rev/${pkg._rev}`, opts.concat({
            method: 'DELETE',
            ignoreBody: true
          }))
        } else if (versionPublic) {
          const latestVer = pkg['dist-tags'].latest
          Object.keys(pkg['dist-tags']).forEach(tag => {
            if (pkg['dist-tags'][tag] === version) {
              delete pkg['dist-tags'][tag]
            }
          })

          if (latestVer === version) {
            pkg['dist-tags'].latest = Object.keys(
              allVersions
            ).sort(semver.compareLoose).pop()
          }

          delete pkg._revisions
          delete pkg._attachments
          // Update packument with removed versions
          return npmFetch(`${pkgUri}/-rev/${pkg._rev}`, opts.concat({
            method: 'PUT',
            body: pkg,
            ignoreBody: true
          })).then(() => {
            // Remove the tarball itself
            return npmFetch.json(pkgUri, opts.concat({
              query: { write: true }
            })).then(({ _rev, _id }) => {
              const tarballUrl = url.parse(dist.tarball).pathname.substr(1)
              return npmFetch(`${tarballUrl}/-rev/${_rev}`, opts.concat({
                method: 'DELETE',
                ignoreBody: true
              }))
            })
          })
        }
      }
    }, err => {
      if (err.code !== 'E404') {
        throw err
      }
    })
  }).then(() => true)
}
