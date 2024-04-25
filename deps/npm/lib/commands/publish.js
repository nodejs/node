const { log, output } = require('proc-log')
const semver = require('semver')
const pack = require('libnpmpack')
const libpub = require('libnpmpublish').publish
const runScript = require('@npmcli/run-script')
const pacote = require('pacote')
const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const { redactLog: replaceInfo } = require('@npmcli/redact')
const otplease = require('../utils/otplease.js')
const { getContents, logTar } = require('../utils/tar.js')
// for historical reasons, publishConfig in package.json can contain ANY config
// keys that npm supports in .npmrc files and elsewhere.  We *may* want to
// revisit this at some point, and have a minimal set that's a SemVer-major
// change that ought to get a RFC written on it.
const { flatten } = require('@npmcli/config/lib/definitions')
const pkgJson = require('@npmcli/package-json')
const BaseCommand = require('../base-cmd.js')

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
    'provenance',
  ]

  static usage = ['<package-spec>']
  static workspaces = true
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

    const opts = { ...this.npm.flatOptions, progress: false }

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
      })
    }

    // we pass dryRun: true to libnpmpack so it doesn't write the file to disk
    const tarballData = await pack(spec, {
      ...opts,
      foregroundScripts: this.npm.config.isDefault('foreground-scripts')
        ? true
        : this.npm.config.get('foreground-scripts'),
      dryRun: true,
      prefix: this.npm.localPrefix,
      workspaces: this.workspacePaths,
    })
    const pkgContents = await getContents(manifest, tarballData)

    // The purpose of re-reading the manifest is in case it changed,
    // so that we send the latest and greatest thing to the registry
    // note that publishConfig might have changed as well!
    manifest = await this.getManifest(spec, opts, true)

    // JSON already has the package contents
    if (!json) {
      logTar(pkgContents, { unicode })
    }

    const resolved = npa.resolve(manifest.name, manifest.version)
    const registry = npmFetch.pickRegistry(resolved, opts)
    const creds = this.npm.config.getCredentialsByURI(registry)
    const noCreds = !(creds.token || creds.username || creds.certfile && creds.keyfile)
    const outputRegistry = replaceInfo(registry)

    if (noCreds) {
      const msg = `This command requires you to be logged in to ${outputRegistry}`
      if (dryRun) {
        log.warn('', `${msg} (dry-run)`)
      } else {
        throw Object.assign(new Error(msg), { code: 'ENEEDAUTH' })
      }
    }

    const access = opts.access === null ? 'default' : opts.access
    let msg = `Publishing to ${outputRegistry} with tag ${defaultTag} and ${access} access`
    if (dryRun) {
      msg = `${msg} (dry-run)`
    }

    log.notice('', msg)

    if (!dryRun) {
      await otplease(this.npm, opts, o => libpub(manifest, tarballData, o))
    }

    if (spec.type === 'directory' && !ignoreScripts) {
      await runScript({
        event: 'publish',
        path: spec.fetchSpec,
        stdio: 'inherit',
        pkg: manifest,
      })

      await runScript({
        event: 'postpublish',
        path: spec.fetchSpec,
        stdio: 'inherit',
        pkg: manifest,
      })
    }

    if (!this.suppressOutput) {
      if (!silent && json) {
        output.standard(JSON.stringify(pkgContents, null, 2))
      } else if (!silent) {
        output.standard(`+ ${pkgContents.id}`)
      }
    }

    return pkgContents
  }

  async execWorkspaces (args) {
    // Suppresses JSON output in publish() so we can handle it here
    this.suppressOutput = true

    const results = {}
    const json = this.npm.config.get('json')
    const { silent } = this.npm
    await this.setWorkspaces()

    for (const [name, workspace] of this.workspaces.entries()) {
      let pkgContents
      try {
        pkgContents = await this.exec([workspace])
      } catch (err) {
        if (err.code === 'EPRIVATE') {
          log.warn(
            'publish',
            `Skipping workspace ${
              this.npm.chalk.cyan(name)
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
        output.standard(`+ ${pkgContents.id}`)
      } else {
        results[name] = pkgContents
      }
    }

    if (!silent && json) {
      output.standard(JSON.stringify(results, null, 2))
    }
  }

  // if it's a directory, read it from the file system
  // otherwise, get the full metadata from whatever it is
  // XXX can't pacote read the manifest from a directory?
  async getManifest (spec, opts, logWarnings = false) {
    let manifest
    if (spec.type === 'directory') {
      const changes = []
      const pkg = await pkgJson.fix(spec.fetchSpec, { changes })
      if (changes.length && logWarnings) {
        /* eslint-disable-next-line max-len */
        log.warn('publish', 'npm auto-corrected some errors in your package.json when publishing.  Please run "npm pkg fix" to address these errors.')
        log.warn('publish', `errors corrected:\n${changes.join('\n')}`)
      }
      // Prepare is the special function for publishing, different than normalize
      const { content } = await pkg.prepare()
      manifest = content
    } else {
      manifest = await pacote.manifest(spec, {
        ...opts,
        fullmetadata: true,
        fullReadJson: true,
      })
    }
    if (manifest.publishConfig) {
      const cliFlags = this.npm.config.data.get('cli').raw
      // Filter out properties set in CLI flags to prioritize them over
      // corresponding `publishConfig` settings
      const filteredPublishConfig = Object.fromEntries(
        Object.entries(manifest.publishConfig).filter(([key]) => !(key in cliFlags)))
      flatten(filteredPublishConfig, opts)
    }
    return manifest
  }
}

module.exports = Publish
