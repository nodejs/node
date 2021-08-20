const path = require('path')
const util = require('util')
const npa = require('npm-package-arg')
const libaccess = require('libnpmaccess')
const npmFetch = require('npm-registry-fetch')
const libunpub = require('libnpmpublish').unpublish
const readJson = util.promisify(require('read-package-json'))

const otplease = require('./utils/otplease.js')
const getIdentity = require('./utils/get-identity.js')

const BaseCommand = require('./base-command.js')
class Unpublish extends BaseCommand {
  static get description () {
    return 'Remove a package from the registry'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'unpublish'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['dry-run', 'force', 'workspace', 'workspaces']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<@scope>/]<pkg>[@<version>]']
  }

  async completion (args) {
    const { partialWord, conf } = args

    if (conf.argv.remain.length >= 3)
      return []

    const opts = this.npm.flatOptions
    const username = await getIdentity(this.npm, { ...opts }).catch(() => null)
    if (!username)
      return []

    const access = await libaccess.lsPackages(username, opts)
    // do a bit of filtering at this point, so that we don't need
    // to fetch versions for more than one thing, but also don't
    // accidentally unpublish a whole project
    let pkgs = Object.keys(access || {})
    if (!partialWord || !pkgs.length)
      return pkgs

    const pp = npa(partialWord).name
    pkgs = pkgs.filter(p => !p.indexOf(pp))
    if (pkgs.length > 1)
      return pkgs

    const json = await npmFetch.json(npa(pkgs[0]).escapedName, opts)
    const versions = Object.keys(json.versions)
    if (!versions.length)
      return pkgs
    else
      return versions.map(v => `${pkgs[0]}@${v}`)
  }

  exec (args, cb) {
    this.unpublish(args).then(() => cb()).catch(cb)
  }

  execWorkspaces (args, filters, cb) {
    this.unpublishWorkspaces(args, filters).then(() => cb()).catch(cb)
  }

  async unpublish (args) {
    if (args.length > 1)
      throw this.usageError()

    const spec = args.length && npa(args[0])
    const force = this.npm.config.get('force')
    const loglevel = this.npm.config.get('loglevel')
    const silent = loglevel === 'silent'
    const dryRun = this.npm.config.get('dry-run')
    let pkgName
    let pkgVersion

    this.npm.log.silly('unpublish', 'args[0]', args[0])
    this.npm.log.silly('unpublish', 'spec', spec)

    if ((!spec || !spec.rawSpec) && !force) {
      throw this.usageError(
        'Refusing to delete entire project.\n' +
        'Run with --force to do this.'
      )
    }

    const opts = this.npm.flatOptions
    if (!spec || path.resolve(spec.name) === this.npm.localPrefix) {
      // if there's a package.json in the current folder, then
      // read the package name and version out of that.
      const pkgJson = path.join(this.npm.localPrefix, 'package.json')
      let manifest
      try {
        manifest = await readJson(pkgJson)
      } catch (err) {
        if (err && err.code !== 'ENOENT' && err.code !== 'ENOTDIR')
          throw err
        else
          throw this.usageError()
      }

      this.npm.log.verbose('unpublish', manifest)

      const { name, version, publishConfig } = manifest
      const pkgJsonSpec = npa.resolve(name, version)
      const optsWithPub = { ...opts, publishConfig }
      if (!dryRun)
        await otplease(opts, opts => libunpub(pkgJsonSpec, optsWithPub))
      pkgName = name
      pkgVersion = version ? `@${version}` : ''
    } else {
      if (!dryRun)
        await otplease(opts, opts => libunpub(spec, opts))
      pkgName = spec.name
      pkgVersion = spec.type === 'version' ? `@${spec.rawSpec}` : ''
    }

    if (!silent)
      this.npm.output(`- ${pkgName}${pkgVersion}`)
  }

  async unpublishWorkspaces (args, filters) {
    await this.setWorkspaces(filters)

    const force = this.npm.config.get('force')
    if (!force) {
      throw this.usageError(
        'Refusing to delete entire project(s).\n' +
        'Run with --force to do this.'
      )
    }

    for (const name of this.workspaceNames)
      await this.unpublish([name])
  }
}
module.exports = Unpublish
