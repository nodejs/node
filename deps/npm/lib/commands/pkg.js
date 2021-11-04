const PackageJson = require('@npmcli/package-json')
const BaseCommand = require('../base-command.js')
const Queryable = require('../utils/queryable.js')

class Pkg extends BaseCommand {
  static get description () {
    return 'Manages your package.json'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'pkg'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return [
      'set <key>=<value> [<key>=<value> ...]',
      'get [<key> [<key> ...]]',
      'delete <key> [<key> ...]',
    ]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'force',
      'json',
      'workspace',
      'workspaces',
    ]
  }

  async exec (args, { prefix } = {}) {
    if (!prefix)
      this.prefix = this.npm.localPrefix
    else
      this.prefix = prefix

    if (this.npm.config.get('global')) {
      throw Object.assign(
        new Error(`There's no package.json file to manage on global mode`),
        { code: 'EPKGGLOBAL' }
      )
    }

    const [cmd, ..._args] = args
    switch (cmd) {
      case 'get':
        return this.get(_args)
      case 'set':
        return this.set(_args)
      case 'delete':
        return this.delete(_args)
      default:
        throw this.usageError()
    }
  }

  async execWorkspaces (args, filters) {
    await this.setWorkspaces(filters)
    const result = {}
    for (const [workspaceName, workspacePath] of this.workspaces.entries()) {
      this.prefix = workspacePath
      result[workspaceName] = await this.exec(args, { prefix: workspacePath })
    }
    // when running in workspaces names, make sure to key by workspace
    // name the results of each value retrieved in each ws
    this.npm.output(JSON.stringify(result, null, 2))
  }

  async get (args) {
    const pkgJson = await PackageJson.load(this.prefix)

    const { content } = pkgJson
    let result = !args.length && content

    if (!result) {
      const q = new Queryable(content)
      result = q.query(args)

      // in case there's only a single result from the query
      // just prints that one element to stdout
      if (Object.keys(result).length === 1)
        result = result[args]
    }

    // only outputs if not running with workspaces config,
    // in case you're retrieving info for workspaces the pkgWorkspaces
    // will handle the output to make sure it get keyed by ws name
    if (!this.workspaces)
      this.npm.output(JSON.stringify(result, null, 2))

    return result
  }

  async set (args) {
    const setError = () =>
      this.usageError('npm pkg set expects a key=value pair of args.')

    if (!args.length)
      throw setError()

    const force = this.npm.config.get('force')
    const json = this.npm.config.get('json')
    const pkgJson = await PackageJson.load(this.prefix)
    const q = new Queryable(pkgJson.content)
    for (const arg of args) {
      const [key, ...rest] = arg.split('=')
      const value = rest.join('=')
      if (!key || !value)
        throw setError()

      q.set(key, json ? JSON.parse(value) : value, { force })
    }

    pkgJson.update(q.toJSON())
    await pkgJson.save()
  }

  async delete (args) {
    const setError = () =>
      this.usageError('npm pkg delete expects key args.')

    if (!args.length)
      throw setError()

    const pkgJson = await PackageJson.load(this.prefix)
    const q = new Queryable(pkgJson.content)
    for (const key of args) {
      if (!key)
        throw setError()

      q.delete(key)
    }

    pkgJson.update(q.toJSON())
    await pkgJson.save()
  }
}

module.exports = Pkg
