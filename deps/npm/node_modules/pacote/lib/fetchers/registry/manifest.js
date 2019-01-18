'use strict'

const fetch = require('npm-registry-fetch')
const fetchPackument = require('./packument')
const optCheck = require('../../util/opt-check')
const pickManifest = require('npm-pick-manifest')
const ssri = require('ssri')

module.exports = manifest
function manifest (spec, opts) {
  opts = optCheck(opts)

  return getManifest(spec, opts).then(manifest => {
    return annotateManifest(spec, manifest, opts)
  })
}

function getManifest (spec, opts) {
  return fetchPackument(spec, opts.concat({
    fullMetadata: opts.enjoyBy ? true : opts.fullMetadata
  })).then(packument => {
    try {
      return pickManifest(packument, spec.fetchSpec, {
        defaultTag: opts.defaultTag,
        enjoyBy: opts.enjoyBy,
        includeDeprecated: opts.includeDeprecated
      })
    } catch (err) {
      if (err.code === 'ETARGET' && packument._cached && !opts.offline) {
        opts.log.silly(
          'registry:manifest',
          `no matching version for ${spec.name}@${spec.fetchSpec} in the cache. Forcing revalidation`
        )
        opts = opts.concat({
          preferOffline: false,
          preferOnline: true
        })
        return fetchPackument(spec, opts.concat({
          fullMetadata: opts.enjoyBy ? true : opts.fullMetadata
        })).then(packument => {
          return pickManifest(packument, spec.fetchSpec, {
            defaultTag: opts.defaultTag,
            enjoyBy: opts.enjoyBy
          })
        })
      } else {
        throw err
      }
    }
  })
}

function annotateManifest (spec, manifest, opts) {
  const shasum = manifest.dist && manifest.dist.shasum
  manifest._integrity = manifest.dist && manifest.dist.integrity
  manifest._shasum = shasum
  if (!manifest._integrity && shasum) {
    // Use legacy dist.shasum field if available.
    manifest._integrity = ssri.fromHex(shasum, 'sha1').toString()
  }
  manifest._resolved = (
    manifest.dist && manifest.dist.tarball
  )
  if (!manifest._resolved) {
    const registry = fetch.pickRegistry(spec, opts)
    const uri = registry.replace(/\/?$/, '/') + spec.escapedName

    const err = new Error(
      `Manifest for ${manifest.name}@${manifest.version} from ${uri} is missing a tarball url (pkg.dist.tarball). Guessing a default.`
    )
    err.code = 'ENOTARBALL'
    err.manifest = manifest
    if (!manifest._warnings) { manifest._warnings = [] }
    manifest._warnings.push(err.message)
    manifest._resolved =
    `${registry}/${manifest.name}/-/${manifest.name}-${manifest.version}.tgz`
  }
  return manifest
}
