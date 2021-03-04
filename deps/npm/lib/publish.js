const util = require('util')
const log = require('npmlog')
const semver = require('semver')
const pack = require('libnpmpack')
const libpub = require('libnpmpublish').publish
const runScript = require('@npmcli/run-script')
const pacote = require('pacote')
const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')

const { flatten } = require('./utils/flat-options.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const usageUtil = require('./utils/usage.js')
const { getContents, logTar } = require('./utils/tar.js')

// this is the only case in the CLI where we use the old full slow
// 'read-package-json' module, because we want to pull in all the
// defaults and metadata, like git sha's and default scripts and all that.
const readJson = util.promisify(require('read-package-json'))

class Publish {
  constructor (npm) {
    this.npm = npm
  }

  get usage () {
    return usageUtil('publish',
      'npm publish [<folder>] [--tag <tag>] [--access <public|restricted>] [--dry-run]' +
      '\n\nPublishes \'.\' if no argument supplied' +
      '\nSets tag `latest` if no --tag specified')
  }

  exec (args, cb) {
    this.publish(args).then(() => cb()).catch(cb)
  }

  async publish (args) {
    if (args.length === 0)
      args = ['.']
    if (args.length !== 1)
      throw this.usage

    log.verbose('publish', args)

    const opts = { ...this.npm.flatOptions }
    const { unicode, dryRun, json, defaultTag } = opts

    if (semver.validRange(defaultTag))
      throw new Error('Tag name must not be a valid SemVer range: ' + defaultTag.trim())

    // you can publish name@version, ./foo.tgz, etc.
    // even though the default is the 'file:.' cwd.
    const spec = npa(args[0])
    let manifest = await this.getManifest(spec, opts)

    if (manifest.publishConfig)
      Object.assign(opts, this.publishConfigToOpts(manifest.publishConfig))

    // only run scripts for directory type publishes
    if (spec.type === 'directory') {
      await runScript({
        event: 'prepublishOnly',
        path: spec.fetchSpec,
        stdio: 'inherit',
        pkg: manifest,
        banner: log.level !== 'silent',
      })
    }

    const tarballData = await pack(spec, opts)
    const pkgContents = await getContents(manifest, tarballData)

    // The purpose of re-reading the manifest is in case it changed,
    // so that we send the latest and greatest thing to the registry
    // note that publishConfig might have changed as well!
    manifest = await this.getManifest(spec, opts)
    if (manifest.publishConfig)
      Object.assign(opts, this.publishConfigToOpts(manifest.publishConfig))

    // note that logTar calls npmlog.notice(), so if we ARE in silent mode,
    // this will do nothing, but we still want it in the debuglog if it fails.
    if (!json)
      logTar(pkgContents, { log, unicode })

    if (!dryRun) {
      const resolved = npa.resolve(manifest.name, manifest.version)
      const registry = npmFetch.pickRegistry(resolved, opts)
      const creds = this.npm.config.getCredentialsByURI(registry)
      if (!creds.token && !creds.username) {
        throw Object.assign(new Error('This command requires you to be logged in.'), {
          code: 'ENEEDAUTH',
        })
      }
      await otplease(opts, opts => libpub(manifest, tarballData, opts))
    }

    if (spec.type === 'directory') {
      await runScript({
        event: 'publish',
        path: spec.fetchSpec,
        stdio: 'inherit',
        pkg: manifest,
        banner: log.level !== 'silent',
      })

      await runScript({
        event: 'postpublish',
        path: spec.fetchSpec,
        stdio: 'inherit',
        pkg: manifest,
        banner: log.level !== 'silent',
      })
    }

    const silent = log.level === 'silent'
    if (!silent && json)
      output(JSON.stringify(pkgContents, null, 2))
    else if (!silent)
      output(`+ ${pkgContents.id}`)

    return pkgContents
  }

  // if it's a directory, read it from the file system
  // otherwise, get the full metadata from whatever it is
  getManifest (spec, opts) {
    if (spec.type === 'directory')
      return readJson(`${spec.fetchSpec}/package.json`)
    return pacote.manifest(spec, { ...opts, fullMetadata: true })
  }

  // for historical reasons, publishConfig in package.json can contain
  // ANY config keys that npm supports in .npmrc files and elsewhere.
  // We *may* want to revisit this at some point, and have a minimal set
  // that's a SemVer-major change that ought to get a RFC written on it.
  publishConfigToOpts (publishConfig) {
    // create a new object that inherits from the config stack
    // then squash the css-case into camelCase opts, like we do
    return flatten({...this.npm.config.list[0], ...publishConfig})
  }
}
module.exports = Publish
