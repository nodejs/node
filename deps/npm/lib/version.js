const libversion = require('libnpmversion')

const BaseCommand = require('./base-command.js')
class Version extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'version'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<newversion> | major | minor | patch | premajor | preminor | prepatch | prerelease [--preid=<prerelease-id>] | from-git]']
  }

  async completion (opts) {
    const { conf: { argv: { remain } } } = opts
    if (remain.length > 2)
      return []

    return [
      'major',
      'minor',
      'patch',
      'premajor',
      'preminor',
      'prepatch',
      'prerelease',
      'from-git',
    ]
  }

  exec (args, cb) {
    return this.version(args).then(() => cb()).catch(cb)
  }

  async version (args) {
    switch (args.length) {
      case 0:
        return this.list()
      case 1:
        return this.change(args)
      default:
        throw this.usage
    }
  }

  async change (args) {
    const prefix = this.npm.flatOptions.tagVersionPrefix
    const version = await libversion(args[0], {
      ...this.npm.flatOptions,
      path: this.npm.prefix,
    })
    return this.npm.output(`${prefix}${version}`)
  }

  async list () {
    const results = {}
    const { promisify } = require('util')
    const { resolve } = require('path')
    const readFile = promisify(require('fs').readFile)
    const pj = resolve(this.npm.prefix, 'package.json')

    const pkg = await readFile(pj, 'utf8')
      .then(data => JSON.parse(data))
      .catch(() => ({}))

    if (pkg.name && pkg.version)
      results[pkg.name] = pkg.version

    results.npm = this.npm.version
    for (const [key, version] of Object.entries(process.versions))
      results[key] = version

    if (this.npm.flatOptions.json)
      this.npm.output(JSON.stringify(results, null, 2))
    else
      this.npm.output(results)
  }
}
module.exports = Version
