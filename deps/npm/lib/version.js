const libnpmversion = require('libnpmversion')
const { resolve } = require('path')
const { promisify } = require('util')
const readFile = promisify(require('fs').readFile)

const getWorkspaces = require('./workspaces/get-workspaces.js')
const BaseCommand = require('./base-command.js')

class Version extends BaseCommand {
  static get description () {
    return 'Bump a package version'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'version'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'allow-same-version',
      'commit-hooks',
      'git-tag-version',
      'json',
      'preid',
      'sign-git-tag',
      'workspace',
      'workspaces',
    ]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<newversion> | major | minor | patch | premajor | preminor | prepatch | prerelease | from-git]']
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

  execWorkspaces (args, filters, cb) {
    this.versionWorkspaces(args, filters).then(() => cb()).catch(cb)
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

  async versionWorkspaces (args, filters) {
    switch (args.length) {
      case 0:
        return this.listWorkspaces(filters)
      case 1:
        return this.changeWorkspaces(args, filters)
      default:
        throw this.usage
    }
  }

  async change (args) {
    const prefix = this.npm.config.get('tag-version-prefix')
    const version = await libnpmversion(args[0], {
      ...this.npm.flatOptions,
      path: this.npm.prefix,
    })
    return this.npm.output(`${prefix}${version}`)
  }

  async changeWorkspaces (args, filters) {
    const prefix = this.npm.config.get('tag-version-prefix')
    const workspaces =
      await getWorkspaces(filters, { path: this.npm.localPrefix })
    for (const [name, path] of workspaces) {
      this.npm.output(name)
      const version = await libnpmversion(args[0], {
        ...this.npm.flatOptions,
        'git-tag-version': false,
        path,
      })
      this.npm.output(`${prefix}${version}`)
    }
  }

  async list (results = {}) {
    const pj = resolve(this.npm.prefix, 'package.json')

    const pkg = await readFile(pj, 'utf8')
      .then(data => JSON.parse(data))
      .catch(() => ({}))

    if (pkg.name && pkg.version)
      results[pkg.name] = pkg.version

    results.npm = this.npm.version
    for (const [key, version] of Object.entries(process.versions))
      results[key] = version

    if (this.npm.config.get('json'))
      this.npm.output(JSON.stringify(results, null, 2))
    else
      this.npm.output(results)
  }

  async listWorkspaces (filters) {
    const results = {}
    const workspaces =
      await getWorkspaces(filters, { path: this.npm.localPrefix })
    for (const [, path] of workspaces) {
      const pj = resolve(path, 'package.json')
      // getWorkspaces has already parsed this so we know it won't error
      const pkg = await readFile(pj, 'utf8')
        .then(data => JSON.parse(data))

      if (pkg.name && pkg.version)
        results[pkg.name] = pkg.version
    }
    return this.list(results)
  }
}

module.exports = Version
