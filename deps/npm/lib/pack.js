const util = require('util')
const log = require('npmlog')
const pacote = require('pacote')
const libpack = require('libnpmpack')
const npa = require('npm-package-arg')
const getWorkspaces = require('./workspaces/get-workspaces.js')

const { getContents, logTar } = require('./utils/tar.js')

const writeFile = util.promisify(require('fs').writeFile)

const BaseCommand = require('./base-command.js')

class Pack extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Create a tarball from a package'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'pack'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['dry-run', 'workspace', 'workspaces']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<pkg>...]']
  }

  exec (args, cb) {
    this.pack(args).then(() => cb()).catch(cb)
  }

  execWorkspaces (args, filters, cb) {
    this.packWorkspaces(args, filters).then(() => cb()).catch(cb)
  }

  async pack (args) {
    if (args.length === 0)
      args = ['.']

    const unicode = this.npm.config.get('unicode')

    // clone the opts because pacote mutates it with resolved/integrity
    const tarballs = await Promise.all(args.map(async (arg) => {
      const spec = npa(arg)
      const dryRun = this.npm.config.get('dry-run')
      const manifest = await pacote.manifest(spec, this.npm.flatOptions)
      const filename = `${manifest.name}-${manifest.version}.tgz`
        .replace(/^@/, '').replace(/\//, '-')
      const tarballData = await libpack(arg, this.npm.flatOptions)
      const pkgContents = await getContents(manifest, tarballData)

      if (!dryRun)
        await writeFile(filename, tarballData)

      return pkgContents
    }))

    for (const tar of tarballs) {
      logTar(tar, { log, unicode })
      this.npm.output(tar.filename.replace(/^@/, '').replace(/\//, '-'))
    }
  }

  async packWorkspaces (args, filters) {
    // If they either ask for nothing, or explicitly include '.' in the args,
    // we effectively translate that into each workspace requested

    const useWorkspaces = args.length === 0 || args.includes('.')

    if (!useWorkspaces) {
      this.npm.log.warn('Ignoring workspaces for specified package(s)')
      return this.pack(args)
    }

    const workspaces =
      await getWorkspaces(filters, { path: this.npm.localPrefix })
    return this.pack([...workspaces.values(), ...args.filter(a => a !== '.')])
  }
}
module.exports = Pack
