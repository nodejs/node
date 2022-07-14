const libaccess = require('libnpmaccess')
const libunpub = require('libnpmpublish').unpublish
const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const path = require('path')
const util = require('util')
const readJson = util.promisify(require('read-package-json'))

const flatten = require('../utils/config/flatten.js')
const getIdentity = require('../utils/get-identity.js')
const log = require('../utils/log-shim')
const otplease = require('../utils/otplease.js')

const LAST_REMAINING_VERSION_ERROR = 'Refusing to delete the last version of the package. ' +
'It will block from republishing a new version for 24 hours.\n' +
'Run with --force to do this.'

const BaseCommand = require('../base-command.js')
class Unpublish extends BaseCommand {
  static description = 'Remove a package from the registry'
  static name = 'unpublish'
  static params = ['dry-run', 'force', 'workspace', 'workspaces']
  static usage = ['[<package-spec>]']
  static ignoreImplicitWorkspace = false

  async getKeysOfVersions (name, opts) {
    const pkgUri = npa(name).escapedName
    const json = await npmFetch.json(`${pkgUri}?write=true`, opts)
    return Object.keys(json.versions)
  }

  async completion (args) {
    const { partialWord, conf } = args

    if (conf.argv.remain.length >= 3) {
      return []
    }

    const opts = { ...this.npm.flatOptions }
    const username = await getIdentity(this.npm, { ...opts }).catch(() => null)
    if (!username) {
      return []
    }

    const access = await libaccess.lsPackages(username, opts)
    // do a bit of filtering at this point, so that we don't need
    // to fetch versions for more than one thing, but also don't
    // accidentally unpublish a whole project
    let pkgs = Object.keys(access || {})
    if (!partialWord || !pkgs.length) {
      return pkgs
    }

    const pp = npa(partialWord).name
    pkgs = pkgs.filter(p => !p.indexOf(pp))
    if (pkgs.length > 1) {
      return pkgs
    }

    const versions = await this.getKeysOfVersions(pkgs[0], opts)
    if (!versions.length) {
      return pkgs
    } else {
      return versions.map(v => `${pkgs[0]}@${v}`)
    }
  }

  async exec (args) {
    if (args.length > 1) {
      throw this.usageError()
    }

    let spec = args.length && npa(args[0])
    const force = this.npm.config.get('force')
    const { silent } = this.npm
    const dryRun = this.npm.config.get('dry-run')

    log.silly('unpublish', 'args[0]', args[0])
    log.silly('unpublish', 'spec', spec)

    if ((!spec || !spec.rawSpec) && !force) {
      throw this.usageError(
        'Refusing to delete entire project.\n' +
        'Run with --force to do this.'
      )
    }

    const opts = { ...this.npm.flatOptions }

    let pkgName
    let pkgVersion
    let manifest
    let manifestErr
    try {
      const pkgJson = path.join(this.npm.localPrefix, 'package.json')
      manifest = await readJson(pkgJson)
    } catch (err) {
      manifestErr = err
    }
    if (spec) {
      // If cwd has a package.json with a name that matches the package being
      // unpublished, load up the publishConfig
      if (manifest && manifest.name === spec.name && manifest.publishConfig) {
        flatten(manifest.publishConfig, opts)
      }
      const versions = await this.getKeysOfVersions(spec.name, opts)
      if (versions.length === 1 && !force) {
        throw this.usageError(LAST_REMAINING_VERSION_ERROR)
      }
      pkgName = spec.name
      pkgVersion = spec.type === 'version' ? `@${spec.rawSpec}` : ''
    } else {
      if (manifestErr) {
        if (manifestErr.code === 'ENOENT' || manifestErr.code === 'ENOTDIR') {
          throw this.usageError()
        } else {
          throw manifestErr
        }
      }

      log.verbose('unpublish', manifest)

      spec = npa.resolve(manifest.name, manifest.version)
      if (manifest.publishConfig) {
        flatten(manifest.publishConfig, opts)
      }

      pkgName = manifest.name
      pkgVersion = manifest.version ? `@${manifest.version}` : ''
    }

    if (!dryRun) {
      await otplease(opts, opts => libunpub(spec, opts))
    }
    if (!silent) {
      this.npm.output(`- ${pkgName}${pkgVersion}`)
    }
  }

  async execWorkspaces (args, filters) {
    await this.setWorkspaces(filters)

    const force = this.npm.config.get('force')
    if (!force) {
      throw this.usageError(
        'Refusing to delete entire project(s).\n' +
        'Run with --force to do this.'
      )
    }

    for (const name of this.workspaceNames) {
      await this.exec([name])
    }
  }
}
module.exports = Unpublish
