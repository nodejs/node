'use strict'

const cloneDeep = require('lodash.clonedeep')
const figgyPudding = require('figgy-pudding')
const { fixer } = require('normalize-package-data')
const getStream = require('get-stream')
const npa = require('npm-package-arg')
const npmAuth = require('npm-registry-fetch/auth.js')
const npmFetch = require('npm-registry-fetch')
const semver = require('semver')
const ssri = require('ssri')
const url = require('url')
const validate = require('aproba')

const PublishConfig = figgyPudding({
  access: {},
  algorithms: { default: ['sha512'] },
  npmVersion: {},
  tag: { default: 'latest' },
  Promise: { default: () => Promise }
})

module.exports = publish
function publish (manifest, tarball, opts) {
  opts = PublishConfig(opts)
  return new opts.Promise(resolve => resolve()).then(() => {
    validate('OSO|OOO', [manifest, tarball, opts])
    if (manifest.private) {
      throw Object.assign(new Error(
        'This package has been marked as private\n' +
        "Remove the 'private' field from the package.json to publish it."
      ), { code: 'EPRIVATE' })
    }
    const spec = npa.resolve(manifest.name, manifest.version)
    // NOTE: spec is used to pick the appropriate registry/auth combo.
    opts = opts.concat(manifest.publishConfig, { spec })
    const reg = npmFetch.pickRegistry(spec, opts)
    const auth = npmAuth(reg, opts)
    const pubManifest = patchedManifest(spec, auth, manifest, opts)

    // registry-frontdoor cares about the access level, which is only
    // configurable for scoped packages
    if (!spec.scope && opts.access === 'restricted') {
      throw Object.assign(
        new Error("Can't restrict access to unscoped packages."),
        { code: 'EUNSCOPED' }
      )
    }

    return slurpTarball(tarball, opts).then(tardata => {
      const metadata = buildMetadata(
        spec, auth, reg, pubManifest, tardata, opts
      )
      return npmFetch(spec.escapedName, opts.concat({
        method: 'PUT',
        body: metadata,
        ignoreBody: true
      })).catch(err => {
        if (err.code !== 'E409') { throw err }
        return npmFetch.json(spec.escapedName, opts.concat({
          query: { write: true }
        })).then(
          current => patchMetadata(current, metadata, opts)
        ).then(newMetadata => {
          return npmFetch(spec.escapedName, opts.concat({
            method: 'PUT',
            body: newMetadata,
            ignoreBody: true
          }))
        })
      })
    })
  }).then(() => true)
}

function patchedManifest (spec, auth, base, opts) {
  const manifest = cloneDeep(base)
  manifest._nodeVersion = process.versions.node
  if (opts.npmVersion) {
    manifest._npmVersion = opts.npmVersion
  }
  if (auth.username || auth.email) {
    // NOTE: This is basically pointless, but reproduced because it's what
    // legacy does: tl;dr `auth.username` and `auth.email` are going to be
    // undefined in any auth situation that uses tokens instead of plain
    // auth. I can only assume some registries out there decided that
    // _npmUser would be of any use to them, but _npmUser in packuments
    // currently gets filled in by the npm registry itself, based on auth
    // information.
    manifest._npmUser = {
      name: auth.username,
      email: auth.email
    }
  }

  fixer.fixNameField(manifest, { strict: true, allowLegacyCase: true })
  const version = semver.clean(manifest.version)
  if (!version) {
    throw Object.assign(
      new Error('invalid semver: ' + manifest.version),
      { code: 'EBADSEMVER' }
    )
  }
  manifest.version = version
  return manifest
}

function buildMetadata (spec, auth, registry, manifest, tardata, opts) {
  const root = {
    _id: manifest.name,
    name: manifest.name,
    description: manifest.description,
    'dist-tags': {},
    versions: {},
    readme: manifest.readme || ''
  }

  if (opts.access) root.access = opts.access

  if (!auth.token) {
    root.maintainers = [{ name: auth.username, email: auth.email }]
    manifest.maintainers = JSON.parse(JSON.stringify(root.maintainers))
  }

  root.versions[ manifest.version ] = manifest
  const tag = manifest.tag || opts.tag
  root['dist-tags'][tag] = manifest.version

  const tbName = manifest.name + '-' + manifest.version + '.tgz'
  const tbURI = manifest.name + '/-/' + tbName
  const integrity = ssri.fromData(tardata, {
    algorithms: [...new Set(['sha1'].concat(opts.algorithms))]
  })

  manifest._id = manifest.name + '@' + manifest.version
  manifest.dist = manifest.dist || {}
  // Don't bother having sha1 in the actual integrity field
  manifest.dist.integrity = integrity['sha512'][0].toString()
  // Legacy shasum support
  manifest.dist.shasum = integrity['sha1'][0].hexDigest()
  manifest.dist.tarball = url.resolve(registry, tbURI)
    .replace(/^https:\/\//, 'http://')

  root._attachments = {}
  root._attachments[ tbName ] = {
    'content_type': 'application/octet-stream',
    'data': tardata.toString('base64'),
    'length': tardata.length
  }

  return root
}

function patchMetadata (current, newData, opts) {
  const curVers = Object.keys(current.versions || {}).map(v => {
    return semver.clean(v, true)
  }).concat(Object.keys(current.time || {}).map(v => {
    if (semver.valid(v, true)) { return semver.clean(v, true) }
  })).filter(v => v)

  const newVersion = Object.keys(newData.versions)[0]

  if (curVers.indexOf(newVersion) !== -1) {
    throw ConflictError(newData.name, newData.version)
  }

  current.versions = current.versions || {}
  current.versions[newVersion] = newData.versions[newVersion]
  for (var i in newData) {
    switch (i) {
      // objects that copy over the new stuffs
      case 'dist-tags':
      case 'versions':
      case '_attachments':
        for (var j in newData[i]) {
          current[i] = current[i] || {}
          current[i][j] = newData[i][j]
        }
        break

      // ignore these
      case 'maintainers':
        break

      // copy
      default:
        current[i] = newData[i]
    }
  }
  const maint = newData.maintainers && JSON.parse(JSON.stringify(newData.maintainers))
  newData.versions[newVersion].maintainers = maint
  return current
}

function slurpTarball (tarSrc, opts) {
  if (Buffer.isBuffer(tarSrc)) {
    return opts.Promise.resolve(tarSrc)
  } else if (typeof tarSrc === 'string') {
    return opts.Promise.resolve(Buffer.from(tarSrc, 'base64'))
  } else if (typeof tarSrc.pipe === 'function') {
    return getStream.buffer(tarSrc)
  } else {
    return opts.Promise.reject(Object.assign(
      new Error('invalid tarball argument. Must be a Buffer, a base64 string, or a binary stream'), {
        code: 'EBADTAR'
      }))
  }
}

function ConflictError (pkgid, version) {
  return Object.assign(new Error(
    `Cannot publish ${pkgid}@${version} over existing version.`
  ), {
    code: 'EPUBLISHCONFLICT',
    pkgid,
    version
  })
}
