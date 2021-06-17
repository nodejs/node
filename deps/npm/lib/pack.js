const util = require('util')
const log = require('npmlog')
const pacote = require('pacote')
const libpack = require('libnpmpack')
const npa = require('npm-package-arg')
const path = require('path')

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
    return [
      'dry-run',
      'json',
      'pack-destination',
      'workspace',
      'workspaces',
    ]
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
    const dryRun = this.npm.config.get('dry-run')
    const json = this.npm.config.get('json')

    // Get the manifests and filenames first so we can bail early on manifest
    // errors before making any tarballs
    const manifests = []
    for (const arg of args) {
      const spec = npa(arg)
      const manifest = await pacote.manifest(spec, this.npm.flatOptions)
      if (!manifest._id)
        throw new Error('Invalid package, must have name and version')

      const filename = `${manifest.name}-${manifest.version}.tgz`
        .replace(/^@/, '').replace(/\//, '-')
      manifests.push({ arg, filename, manifest })
    }

    // Load tarball names up for printing afterward to isolate from the
    // noise generated during packing
    const tarballs = []
    for (const { arg, filename, manifest } of manifests) {
      const tarballData = await libpack(arg, this.npm.flatOptions)
      const pkgContents = await getContents(manifest, tarballData)
      const tarballFilename = path.resolve(this.npm.config.get('pack-destination'), filename)

      if (!dryRun)
        await writeFile(tarballFilename, tarballData)

      tarballs.push(pkgContents)
    }

    if (json) {
      this.npm.output(JSON.stringify(tarballs, null, 2))
      return
    }

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

    await this.setWorkspaces(filters)
    return this.pack([...this.workspacePaths, ...args.filter(a => a !== '.')])
  }
}
module.exports = Pack
