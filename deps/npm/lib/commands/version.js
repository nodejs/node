const libnpmversion = require('libnpmversion')
const { resolve } = require('path')
const { promisify } = require('util')
const readFile = promisify(require('fs').readFile)

const Arborist = require('@npmcli/arborist')
const reifyFinish = require('../utils/reify-finish.js')

const BaseCommand = require('../base-command.js')

class Version extends BaseCommand {
  static description = 'Bump a package version'
  static name = 'version'
  static params = [
    'allow-same-version',
    'commit-hooks',
    'git-tag-version',
    'json',
    'preid',
    'sign-git-tag',
    'workspace',
    'workspaces',
    'workspaces-update',
    'include-workspace-root',
  ]

  static ignoreImplicitWorkspace = false

  /* eslint-disable-next-line max-len */
  static usage = ['[<newversion> | major | minor | patch | premajor | preminor | prepatch | prerelease | from-git]']

  async completion (opts) {
    const {
      conf: {
        argv: { remain },
      },
    } = opts
    if (remain.length > 2) {
      return []
    }

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

  async exec (args) {
    switch (args.length) {
      case 0:
        return this.list()
      case 1:
        return this.change(args)
      default:
        throw this.usageError()
    }
  }

  async execWorkspaces (args, filters) {
    switch (args.length) {
      case 0:
        return this.listWorkspaces(filters)
      case 1:
        return this.changeWorkspaces(args, filters)
      default:
        throw this.usageError()
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
    await this.setWorkspaces(filters)
    const updatedWorkspaces = []
    for (const [name, path] of this.workspaces) {
      this.npm.output(name)
      const version = await libnpmversion(args[0], {
        ...this.npm.flatOptions,
        'git-tag-version': false,
        path,
      })
      updatedWorkspaces.push(name)
      this.npm.output(`${prefix}${version}`)
    }
    return this.update(updatedWorkspaces)
  }

  async list (results = {}) {
    const pj = resolve(this.npm.prefix, 'package.json')

    const pkg = await readFile(pj, 'utf8')
      .then(data => JSON.parse(data))
      .catch(() => ({}))

    if (pkg.name && pkg.version) {
      results[pkg.name] = pkg.version
    }

    results.npm = this.npm.version
    for (const [key, version] of Object.entries(process.versions)) {
      results[key] = version
    }

    if (this.npm.config.get('json')) {
      this.npm.output(JSON.stringify(results, null, 2))
    } else {
      this.npm.output(results)
    }
  }

  async listWorkspaces (filters) {
    const results = {}
    await this.setWorkspaces(filters)
    for (const path of this.workspacePaths) {
      const pj = resolve(path, 'package.json')
      // setWorkspaces has already parsed package.json so we know it won't error
      const pkg = await readFile(pj, 'utf8').then(data => JSON.parse(data))

      if (pkg.name && pkg.version) {
        results[pkg.name] = pkg.version
      }
    }
    return this.list(results)
  }

  async update (args) {
    if (!this.npm.flatOptions.workspacesUpdate || !args.length) {
      return
    }

    // default behavior is to not save by default in order to avoid
    // race condition problems when publishing multiple workspaces
    // that have dependencies on one another, it might still be useful
    // in some cases, which then need to set --save
    const save = this.npm.config.isDefault('save')
      ? false
      : this.npm.config.get('save')

    // runs a minimalistic reify update, targetting only the workspaces
    // that had version updates and skipping fund/audit/save
    const opts = {
      ...this.npm.flatOptions,
      audit: false,
      fund: false,
      path: this.npm.localPrefix,
      save,
    }
    const arb = new Arborist(opts)

    await arb.reify({ ...opts, update: args })
    await reifyFinish(this.npm, arb)
  }
}

module.exports = Version
