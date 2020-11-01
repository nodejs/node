const util = require('util')
const log = require('npmlog')
const pacote = require('pacote')
const libpack = require('libnpmpack')
const npa = require('npm-package-arg')

const npm = require('./npm.js')
const { getContents, logTar } = require('./utils/tar.js')

const writeFile = util.promisify(require('fs').writeFile)
const output = require('./utils/output.js')

const completion = require('./utils/completion/none.js')
const usageUtil = require('./utils/usage.js')
const usage = usageUtil('pack', 'npm pack [[<@scope>/]<pkg>...] [--dry-run]')

const cmd = (args, cb) => pack(args).then(() => cb()).catch(cb)

const pack = async (args) => {
  if (args.length === 0)
    args = ['.']

  const { unicode } = npm.flatOptions

  // clone the opts because pacote mutates it with resolved/integrity
  const tarballs = await Promise.all(args.map((arg) =>
    pack_(arg, { ...npm.flatOptions })))

  for (const tar of tarballs) {
    logTar(tar, { log, unicode })
    output(tar.filename.replace(/^@/, '').replace(/\//, '-'))
  }
}

const pack_ = async (arg, opts) => {
  const spec = npa(arg)
  const { dryRun } = opts
  const manifest = await pacote.manifest(spec, opts)
  const filename = `${manifest.name}-${manifest.version}.tgz`
    .replace(/^@/, '').replace(/\//, '-')
  const tarballData = await libpack(arg, opts)
  const pkgContents = await getContents(manifest, tarballData)

  if (!dryRun)
    await writeFile(filename, tarballData)

  return pkgContents
}

module.exports = Object.assign(cmd, { usage, completion })
