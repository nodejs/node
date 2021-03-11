const util = require('util')
const log = require('npmlog')
const pacote = require('pacote')
const libpack = require('libnpmpack')
const npa = require('npm-package-arg')

const { getContents, logTar } = require('./utils/tar.js')

const writeFile = util.promisify(require('fs').writeFile)

const BaseCommand = require('./base-command.js')

class Pack extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'pack'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<pkg>...] [--dry-run]']
  }

  exec (args, cb) {
    this.pack(args).then(() => cb()).catch(cb)
  }

  async pack (args) {
    if (args.length === 0)
      args = ['.']

    const { unicode } = this.npm.flatOptions

    // clone the opts because pacote mutates it with resolved/integrity
    const tarballs = await Promise.all(args.map(async (arg) => {
      const spec = npa(arg)
      const { dryRun } = this.npm.flatOptions
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
}
module.exports = Pack
