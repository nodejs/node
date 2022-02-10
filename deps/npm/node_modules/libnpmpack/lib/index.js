'use strict'

const pacote = require('pacote')
const npa = require('npm-package-arg')
const runScript = require('@npmcli/run-script')
const path = require('path')
const util = require('util')
const writeFile = util.promisify(require('fs').writeFile)

module.exports = pack
async function pack (spec = 'file:.', opts = {}) {
  // gets spec
  spec = npa(spec)

  const manifest = await pacote.manifest(spec, opts)

  // Default to true if no log options passed, set to false if we're in silent
  // mode
  const banner = !opts.log || (opts.log.level !== 'silent')

  if (spec.type === 'directory') {
    // prepack
    await runScript({
      ...opts,
      event: 'prepack',
      path: spec.fetchSpec,
      stdio: 'inherit',
      pkg: manifest,
      banner,
    })
  }

  // packs tarball
  const tarball = await pacote.tarball(manifest._resolved, {
    ...opts,
    integrity: manifest._integrity,
  })

  // check for explicit `false` so the default behavior is to skip writing to disk
  if (opts.dryRun === false) {
    const filename = `${manifest.name}-${manifest.version}.tgz`
      .replace(/^@/, '').replace(/\//, '-')
    const destination = path.resolve(opts.packDestination, filename)
    await writeFile(destination, tarball)
  }

  if (spec.type === 'directory') {
    // postpack
    await runScript({
      ...opts,
      event: 'postpack',
      path: spec.fetchSpec,
      stdio: 'inherit',
      pkg: manifest,
      banner,
      env: {
        npm_package_from: tarball.from,
        npm_package_resolved: tarball.resolved,
        npm_package_integrity: tarball.integrity,
      },
    })
  }

  return tarball
}
