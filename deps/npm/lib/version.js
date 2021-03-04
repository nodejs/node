const libversion = require('libnpmversion')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')

class Version {
  constructor (npm) {
    this.npm = npm
  }

  get usage () {
    return usageUtil(
      'version',
      'npm version [<newversion> | major | minor | patch | premajor | preminor | prepatch | prerelease [--preid=<prerelease-id>] | from-git]\n' +
      '(run in package dir)\n\n' +
      `'npm -v' or 'npm --version' to print npm version (${this.npm.version})\n` +
      `'npm view <pkg> version' to view a package's published version\n` +
      `'npm ls' to inspect current package/dependency versions\n`
    )
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
    return output(`${prefix}${version}`)
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
      output(JSON.stringify(results, null, 2))
    else
      output(results)
  }
}
module.exports = Version
