'use strict'

const pacote = require('pacote')
const npa = require('npm-package-arg')
const runScript = require('@npmcli/run-script')

module.exports = pack
async function pack (spec = 'file:.', opts = {}) {
  // gets spec
  spec = npa(spec)

  const manifest = await pacote.manifest(spec, opts)

  if (spec.type === 'directory') {
    // prepack
    await runScript({
      ...opts,
      event: 'prepack',
      path: spec.fetchSpec,
      stdio: 'inherit',
      pkg: manifest
    })
  }

  // packs tarball
  const tarball = await pacote.tarball(manifest._resolved, {
    ...opts,
    integrity: manifest._integrity
  })

  if (spec.type === 'directory') {
    // postpack
    await runScript({
      ...opts,
      event: 'postpack',
      path: spec.fetchSpec,
      stdio: 'inherit',
      pkg: manifest,
      env: {
        npm_package_from: tarball.from,
        npm_package_resolved: tarball.resolved,
        npm_package_integrity: tarball.integrity
      }
    })
  }

  return tarball
}
