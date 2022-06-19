const util = require('util')
const log = require('../utils/log-shim.js')
const semver = require('semver')
const pack = require('libnpmpack')
const libpub = require('libnpmpublish').publish
const runScript = require('@npmcli/run-script')
const pacote = require('pacote')
const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const replaceInfo = require('../utils/replace-info.js')

const otplease = require('../utils/otplease.js')
const { getContents, logTar } = require('../utils/tar.js')

// for historical reasons, publishConfig in package.json can contain ANY config
// keys that npm supports in .npmrc files and elsewhere.  We *may* want to
// revisit this at some point, and have a minimal set that's a SemVer-major
// change that ought to get a RFC written on it.
const flatten = require('../utils/config/flatten.js')

// this is the only case in the CLI where we want to use the old full slow
// 'read-package-json' module, because we want to pull in all the defaults and
// metadata, like git sha's and default scripts and all that.
const readJson = util.promisify(require('read-package-json'))

const BaseCommand = require('../base-command.js')
class Publish extends BaseCommand {
  static description = 'Publish a package'
  static name = 'publish'
  static params = [
    'tag',
    'access',
    'dry-run',
    'otp',
    'workspace',
    'workspaces',
    'include-workspace-root',
  ]

  static usage = ['[<folder>]']
  static ignoreImplicitWorkspace = false

  async exec (args) {
    if (args.length === 0) {
      args = ['.']
    }
    if (args.length !== 1) {
      throw this.usageError()
    }

    log.verbose('publish', replaceInfo(args))

    const unicode = this.npm.config.get('unicode')
    const dryRun = this.npm.config.get('dry-run')
    const json = this.npm.config.get('json')
    const defaultTag = this.npm.config.get('tag')
    const ignoreScripts = this.npm.config.get('ignore-scripts')
    const { silent } = this.npm

    if (semver.validRange(defaultTag)) {
      throw new Error('Tag name must not be a valid SemVer range: ' + defaultTag.trim())
    }

    const opts = { ...this.npm.flatOptions }

    // you can publish name@version, ./foo.tgz, etc.
    // even though the default is the 'file:.' cwd.
    const spec = npa(args[0])
    let manifest = await this.getManifest(spec, opts)

    // only run scripts for directory type publishes
    if (spec.type === 'directory' && !ignoreScripts) {
      await runScript({
        event: 'prepublishOnly',
        path: spec.fetchSpec,
        stdio: 'inherit',
        pkg: manifest,
        banner: !silent,
      })
    }

    // we pass dryRun: true to libnpmpack so it doesn't write the file to disk
    const tarballData = await pack(spec, {
      ...opts,
      dryRun: true,
      prefix: this.npm.localPrefix,
      workspaces: this.workspacePaths,
    })
    const pkgContents = await getContents(manifest, tarballData)

    // The purpose of re-reading the manifest is in case it changed,
    // so that we send the latest and greatest thing to the registry
    // note that publishConfig might have changed as well!
    manifest = await this.getManifest(spec, opts)

    // JSON already has the package contents
    if (!json) {
      logTar(pkgContents, { unicode })
    }

    const resolved = npa.resolve(manifest.name, manifest.version)
    const registry = npmFetch.pickRegistry(resolved, opts)
    const creds = this.npm.config.getCredentialsByURI(registry)
    const noCreds = !creds.token && !creds.username
    const outputRegistry = replaceInfo(registry)

    if (noCreds) {
      const msg = `This command requires you to be logged in to ${outputRegistry}`
      if (dryRun) {
        log.warn('', `${msg} (dry-run)`)
      } else {
        throw Object.assign(new Error(msg), { code: 'ENEEDAUTH' })
      }
    }

    log.notice('', `Publishing to ${outputRegistry}${dryRun ? ' (dry-run)' : ''}`)

    if (!dryRun) {
      await otplease(opts, opts => libpub(manifest, tarballData, opts))
    }

    if (spec.type === 'directory' && !ignoreScripts) {
      await runScript({
        event: 'publish',
        path: spec.fetchSpec,
        stdio: 'inherit',
        pkg: manifest,
        banner: !silent,
      })

      await runScript({
        event: 'postpublish',
        path: spec.fetchSpec,
        stdio: 'inherit',
        pkg: manifest,
        banner: !silent,
      })
    }

    if (!this.suppressOutput) {
      if (!silent && json) {
        this.npm.output(JSON.stringify(pkgContents, null, 2))
      } else if (!silent) {
        this.npm.output(`+ ${pkgContents.id}`)
      }
    }

    return pkgContents
  }

  async execWorkspaces (args, filters) {
    // Suppresses JSON output in publish() so we can handle it here
    this.suppressOutput = true

    const results = {}
    const json = this.npm.config.get('json')
    const { silent } = this.npm
    await this.setWorkspaces(filters)

    for (const [name, workspace] of this.workspaces.entries()) {
      let pkgContents
      try {
        pkgContents = await this.exec([workspace])
      } catch (err) {
        if (err.code === 'EPRIVATE') {
          log.warn(
            'publish',
            `Skipping workspace ${
              this.npm.chalk.green(name)
            }, marked as ${
              this.npm.chalk.bold('private')
            }`
          )
          continue
        }
        throw err
      }
      // This needs to be in-line w/ the rest of the output that non-JSON
      // publish generates
      if (!silent && !json) {
        this.npm.output(`+ ${pkgContents.id}`)
      } else {
        results[name] = pkgContents
      }
    }

    if (!silent && json) {
      this.npm.output(JSON.stringify(results, null, 2))
    }
  }

  // if it's a directory, read it from the file system
  // otherwise, get the full metadata from whatever it is
  // XXX can't pacote read the manifest from a directory?
  async getManifest (spec, opts) {
    let manifest
    if (spec.type === 'directory') {
      manifest = await readJson(`${spec.fetchSpec}/package.json`)
    } else {
      manifest = await pacote.manifest(spec, {
        ...opts,
        fullmetadata: true,
        fullReadJson: true,
      })
    }
    if (manifest.publishConfig) {
      flatten(manifest.publishConfig, opts)
    }
    return manifest
  }
}
module.exports = Publish
