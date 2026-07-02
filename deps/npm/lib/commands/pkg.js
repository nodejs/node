const { inspect } = require('node:util')
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

  async exec (args, { path = this.npm.localPrefix, workspace } = {}) {
    if (this.npm.global) {
      throw Object.assign(
        new Error(`There's no package.json file to manage on global mode`),
        { code: 'EPKGGLOBAL' }
      )
    }

    const [cmd, ..._args] = args
    switch (cmd) {
      case 'get':
        return this.get(_args, { path, workspace })
      case 'set':
        return this.set(_args, { path, workspace }).then(p => p.save())
      case 'delete':
        return this.delete(_args, { path, workspace }).then(p => p.save())
      case 'fix':
        return PackageJson.fix(path).then(p => p.save())
      default:
        throw this.usageError()
    }
  }

  async execWorkspaces (args) {
    await this.setWorkspaces()
    for (const [workspace, path] of this.workspaces.entries()) {
      await this.exec(args, { path, workspace })
    }
  }

  async get (args, { path, workspace }) {
    const pkgJson = await PackageJson.load(path)
    const json = this.npm.config.get('json')

    // filter out the newline/indent symbols from the package-json object
    let result = JSON.parse(JSON.stringify(pkgJson.content))

    if (args.length) {
      result = new Queryable(result).query(args, { unwrapSingleItemArrays: false })
      if (args.length === 1 && !json && args[0] in result) {
        if (workspace) {
          return output.standard(`${workspace} ${result[args[0]]}`)
        }
        return output.standard(result[args[0]])
      }
    }

    if (json) {
      output.buffer(workspace ? { [workspace]: result } : result)
    } else {
      for (let [f, d] of Object.entries(result)) {
        d = inspect(d, {
          showHidden: false,
          depth: 5,
          colors: this.npm.color,
          maxArrayLength: null,
        })

        if (workspace) {
          output.standard(`${workspace} ${f} = ${d}`)
        } else {
          output.standard(`${f} = ${d}`)
        }
      }
    }
  }

  async set (args, { path }) {
    const setError = () =>
      this.usageError('npm pkg set expects a key=value pair of args.')

    if (!args.length) {
      throw setError()
    }

    const force = this.npm.config.get('force')
    const json = this.npm.config.get('json')
    const pkgJson = await PackageJson.load(path)
    const q = new Queryable(pkgJson.content)
    for (const arg of args) {
      const [key, ...rest] = arg.split('=')
      const value = rest.join('=')
      if (!key || !value) {
        throw setError()
      }

      q.set(key, json ? JSON.parse(value) : value, { force })
    }

    return pkgJson.update(q.toJSON())
  }

  async delete (args, { path }) {
    const setError = () =>
      this.usageError('npm pkg delete expects key args.')

    if (!args.length) {
      throw setError()
    }

    const pkgJson = await PackageJson.load(path)
    const q = new Queryable(pkgJson.content)
    for (const key of args) {
      if (!key) {
        throw setError()
      }

      q.delete(key)
    }

    return pkgJson.update(q.toJSON())
  }
}

module.exports = Pkg
