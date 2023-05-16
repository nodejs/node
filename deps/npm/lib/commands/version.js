const libnpmversion = require('libnpmversion')
const { resolve } = require('path')
const { promisify } = require('util')
const readFile = promisify(require('fs').readFile)

const updateWorkspaces = require('../workspaces/update-workspaces.js')
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

  static workspaces = true
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

  async execWorkspaces (args) {
    switch (args.length) {
      case 0:
        return this.listWorkspaces()
      case 1:
        return this.changeWorkspaces(args)
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

  async changeWorkspaces (args) {
    const prefix = this.npm.config.get('tag-version-prefix')
    await this.setWorkspaces()
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

  async listWorkspaces () {
    const results = {}
    await this.setWorkspaces()
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

  async update (workspaces) {
    const {
      config,
      flatOptions,
      localPrefix,
    } = this.npm

    await updateWorkspaces({
      config,
      flatOptions,
      localPrefix,
      npm: this.npm,
      workspaces,
    })
  }
}

module.exports = Version
