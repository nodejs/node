const { fixer } = require('normalize-package-data')
const npmFetch = require('npm-registry-fetch')
const npa = require('npm-package-arg')
const log = require('proc-log')
const semver = require('semver')
const { URL } = require('url')
const ssri = require('ssri')
const ciInfo = require('ci-info')

const { generateProvenance } = require('./provenance')

const TLOG_BASE_URL = 'https://search.sigstore.dev/'

const publish = async (manifest, tarballData, opts) => {
  if (manifest.private) {
    throw Object.assign(
      new Error(`This package has been marked as private
Remove the 'private' field from the package.json to publish it.`),
      { code: 'EPRIVATE' }
    )
  }

  // spec is used to pick the appropriate registry/auth combo
  const spec = npa.resolve(manifest.name, manifest.version)
  opts = {
    access: 'public',
    algorithms: ['sha512'],
    defaultTag: 'latest',
    ...opts,
    spec,
  }

  const reg = npmFetch.pickRegistry(spec, opts)
  const pubManifest = patchManifest(manifest, opts)

  // registry-frontdoor cares about the access level,
  // which is only configurable for scoped packages
  if (!spec.scope && opts.access === 'restricted') {
    throw Object.assign(
      new Error("Can't restrict access to unscoped packages."),
      { code: 'EUNSCOPED' }
    )
  }

  const { metadata, transparencyLogUrl } = await buildMetadata(
    reg,
    pubManifest,
    tarballData,
    spec,
    opts
  )

  try {
    const res = await npmFetch(spec.escapedName, {
      ...opts,
      method: 'PUT',
      body: metadata,
      ignoreBody: true,
    })
    if (transparencyLogUrl) {
      res.transparencyLogUrl = transparencyLogUrl
    }
    return res
  } catch (err) {
    if (err.code !== 'E409') {
      throw err
    }
    // if E409, we attempt exactly ONE retry, to protect us
    // against malicious activity like trying to publish
    // a bunch of new versions of a package at the same time
    // and/or spamming the registry
    const current = await npmFetch.json(spec.escapedName, {
      ...opts,
      query: { write: true },
    })
    const newMetadata = patchMetadata(current, metadata)
    const res = await npmFetch(spec.escapedName, {
      ...opts,
      method: 'PUT',
      body: newMetadata,
      ignoreBody: true,
    })
    /* istanbul ignore next */
    if (transparencyLogUrl) {
      res.transparencyLogUrl = transparencyLogUrl
    }
    return res
  }
}

const patchManifest = (_manifest, opts) => {
  const { npmVersion } = opts
  // we only update top-level fields, so a shallow clone is fine
  const manifest = { ..._manifest }

  manifest._nodeVersion = process.versions.node
  if (npmVersion) {
    manifest._npmVersion = npmVersion
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

const buildMetadata = async (registry, manifest, tarballData, spec, opts) => {
  const { access, defaultTag, algorithms, provenance } = opts
  const root = {
    _id: manifest.name,
    name: manifest.name,
    description: manifest.description,
    'dist-tags': {},
    versions: {},
    access,
  }

  root.versions[manifest.version] = manifest
  const tag = manifest.tag || defaultTag
  root['dist-tags'][tag] = manifest.version

  const tarballName = `${manifest.name}-${manifest.version}.tgz`
  const provenanceBundleName = `${manifest.name}-${manifest.version}.sigstore`
  const tarballURI = `${manifest.name}/-/${tarballName}`
  const integrity = ssri.fromData(tarballData, {
    algorithms: [...new Set(['sha1'].concat(algorithms))],
  })

  manifest._id = `${manifest.name}@${manifest.version}`
  manifest.dist = { ...manifest.dist }
  // Don't bother having sha1 in the actual integrity field
  manifest.dist.integrity = integrity.sha512[0].toString()
  // Legacy shasum support
  manifest.dist.shasum = integrity.sha1[0].hexDigest()

  // NB: the CLI always fetches via HTTPS if the registry is HTTPS,
  // regardless of what's here.  This makes it so that installing
  // from an HTTP-only mirror doesn't cause problems, though.
  manifest.dist.tarball = new URL(tarballURI, registry).href
    .replace(/^https:\/\//, 'http://')

  root._attachments = {}
  root._attachments[tarballName] = {
    content_type: 'application/octet-stream',
    data: tarballData.toString('base64'),
    length: tarballData.length,
  }

  // Handle case where --provenance flag was set to true
  let transparencyLogUrl
  if (provenance === true) {
    const subject = {
      name: npa.toPurl(spec),
      digest: { sha512: integrity.sha512[0].hexDigest() },
    }

    // Ensure that we're running in GHA, currently the only supported build environment
    if (ciInfo.name !== 'GitHub Actions') {
      throw Object.assign(
        new Error('Automatic provenance generation not supported outside of GitHub Actions'),
        { code: 'EUSAGE' }
      )
    }

    // Ensure that the GHA OIDC token is available
    if (!process.env.ACTIONS_ID_TOKEN_REQUEST_URL) {
      throw Object.assign(
        /* eslint-disable-next-line max-len */
        new Error('Provenance generation in GitHub Actions requires "write" access to the "id-token" permission'),
        { code: 'EUSAGE' }
      )
    }

    // Some registries (e.g. GH packages) require auth to check visibility,
    // and always return 404 when no auth is supplied. In this case we assume
    // the package is always private and require `--access public` to publish
    // with provenance.
    let visibility = { public: false }
    if (opts.provenance === true && opts.access !== 'public') {
      try {
        const res = await npmFetch
          .json(`${registry}/-/package/${spec.escapedName}/visibility`, opts)
        visibility = res
      } catch (err) {
        if (err.code !== 'E404') {
          throw err
        }
      }
    }

    if (!visibility.public && opts.provenance === true && opts.access !== 'public') {
      throw Object.assign(
        /* eslint-disable-next-line max-len */
        new Error("Can't generate provenance for new or private package, you must set `access` to public."),
        { code: 'EUSAGE' }
      )
    }
    const provenanceBundle = await generateProvenance([subject], opts)

    /* eslint-disable-next-line max-len */
    log.notice('publish', 'Signed provenance statement with source and build information from GitHub Actions')

    const tlogEntry = provenanceBundle?.verificationMaterial?.tlogEntries[0]
    /* istanbul ignore else */
    if (tlogEntry) {
      transparencyLogUrl = `${TLOG_BASE_URL}?logIndex=${tlogEntry.logIndex}`
      log.notice(
        'publish',
        `Provenance statement published to transparency log: ${transparencyLogUrl}`
      )
    }

    const serializedBundle = JSON.stringify(provenanceBundle)
    root._attachments[provenanceBundleName] = {
      content_type: provenanceBundle.mediaType,
      data: serializedBundle,
      length: serializedBundle.length,
    }
  }

  return {
    metadata: root,
    transparencyLogUrl,
  }
}

const patchMetadata = (current, newData) => {
  const curVers = Object.keys(current.versions || {})
    .map(v => semver.clean(v, true))
    .concat(Object.keys(current.time || {})
      .map(v => semver.valid(v, true) && semver.clean(v, true))
      .filter(v => v))

  const newVersion = Object.keys(newData.versions)[0]

  if (curVers.indexOf(newVersion) !== -1) {
    const { name: pkgid, version } = newData
    throw Object.assign(
      new Error(
        `Cannot publish ${pkgid}@${version} over existing version.`
      ), {
        code: 'EPUBLISHCONFLICT',
        pkgid,
        version,
      })
  }

  current.versions = current.versions || {}
  current.versions[newVersion] = newData.versions[newVersion]
  for (const i in newData) {
    switch (i) {
      // objects that copy over the new stuffs
      case 'dist-tags':
      case 'versions':
      case '_attachments':
        for (const j in newData[i]) {
          current[i] = current[i] || {}
          current[i][j] = newData[i][j]
        }
        break

      // copy
      default:
        current[i] = newData[i]
        break
    }
  }

  return current
}

module.exports = publish
