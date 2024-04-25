const { output } = require('proc-log')
const PackageJson = require('@npmcli/package-json')
const BaseCommand = require('../base-cmd.js')
const Queryable = require('../utils/queryable.js')

class Pkg extends BaseCommand {
  static description = 'Manages your package.json'
  static name = 'pkg'
  static usage = [
    'set <key>=<value> [<key>=<value> ...]',
    'get [<key> [<key> ...]]',
    'delete <key> [<key> ...]',
    'set [<array>[<index>].<key>=<value> ...]',
    'set [<array>[].<key>=<value> ...]',
    'fix',
  ]

  static params = [
    'force',
    'json',
    'workspace',
    'workspaces',
  ]

  static workspaces = true
  static ignoreImplicitWorkspace = false

  async exec (args, { prefix } = {}) {
    if (!prefix) {
      this.prefix = this.npm.localPrefix
    } else {
      this.prefix = prefix
    }

    if (this.npm.global) {
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
      case 'fix':
        return this.fix(_args)
      default:
        throw this.usageError()
    }
  }

  async execWorkspaces (args) {
    await this.setWorkspaces()
    const result = {}
    for (const [workspaceName, workspacePath] of this.workspaces.entries()) {
      this.prefix = workspacePath
      result[workspaceName] = await this.exec(args, { prefix: workspacePath })
    }
    // when running in workspaces names, make sure to key by workspace
    // name the results of each value retrieved in each ws
    output.standard(JSON.stringify(result, null, 2))
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
      if (Object.keys(result).length === 1) {
        result = result[args]
      }
    }

    // only outputs if not running with workspaces config
    // execWorkspaces will handle the output otherwise
    if (!this.workspaces) {
      output.standard(JSON.stringify(result, null, 2))
    }

    return result
  }

  async set (args) {
    const setError = () =>
      this.usageError('npm pkg set expects a key=value pair of args.')

    if (!args.length) {
      throw setError()
    }

    const force = this.npm.config.get('force')
    const json = this.npm.config.get('json')
    const pkgJson = await PackageJson.load(this.prefix)
    const q = new Queryable(pkgJson.content)
    for (const arg of args) {
      const [key, ...rest] = arg.split('=')
      const value = rest.join('=')
      if (!key || !value) {
        throw setError()
      }

      q.set(key, json ? JSON.parse(value) : value, { force })
    }

    pkgJson.update(q.toJSON())
    await pkgJson.save()
  }

  async delete (args) {
    const setError = () =>
      this.usageError('npm pkg delete expects key args.')

    if (!args.length) {
      throw setError()
    }

    const pkgJson = await PackageJson.load(this.prefix)
    const q = new Queryable(pkgJson.content)
    for (const key of args) {
      if (!key) {
        throw setError()
      }

      q.delete(key)
    }

    pkgJson.update(q.toJSON())
    await pkgJson.save()
  }

  async fix () {
    const pkgJson = await PackageJson.fix(this.prefix)
    await pkgJson.save()
  }
}

module.exports = Pkg
