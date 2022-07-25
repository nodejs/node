const path = require('path')

const libaccess = require('libnpmaccess')
const readPackageJson = require('read-package-json-fast')

const otplease = require('../utils/otplease.js')
const getIdentity = require('../utils/get-identity.js')
const BaseCommand = require('../base-command.js')

const subcommands = [
  'public',
  'restricted',
  'grant',
  'revoke',
  'ls-packages',
  'ls-collaborators',
  'edit',
  '2fa-required',
  '2fa-not-required',
]

class Access extends BaseCommand {
  static description = 'Set access level on published packages'
  static name = 'access'
  static params = [
    'registry',
    'otp',
  ]

  static ignoreImplicitWorkspace = true

  static usage = [
    'public [<package>]',
    'restricted [<package>]',
    'grant <read-only|read-write> <scope:team> [<package>]',
    'revoke <scope:team> [<package>]',
    '2fa-required [<package>]',
    '2fa-not-required [<package>]',
    'ls-packages [<user>|<scope>|<scope:team>]',
    'ls-collaborators [<package> [<user>]]',
    'edit [<package>]',
  ]

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2) {
      return subcommands
    }

    switch (argv[2]) {
      case 'grant':
        if (argv.length === 3) {
          return ['read-only', 'read-write']
        } else {
          return []
        }

      case 'public':
      case 'restricted':
      case 'ls-packages':
      case 'ls-collaborators':
      case 'edit':
      case '2fa-required':
      case '2fa-not-required':
      case 'revoke':
        return []
      default:
        throw new Error(argv[2] + ' not recognized')
    }
  }

  async exec ([cmd, ...args]) {
    if (!cmd) {
      throw this.usageError('Subcommand is required.')
    }

    if (!subcommands.includes(cmd) || !this[cmd]) {
      throw this.usageError(`${cmd} is not a recognized subcommand.`)
    }

    return this[cmd](args, {
      ...this.npm.flatOptions,
    })
  }

  public ([pkg], opts) {
    return this.modifyPackage(pkg, opts, libaccess.public)
  }

  restricted ([pkg], opts) {
    return this.modifyPackage(pkg, opts, libaccess.restricted)
  }

  async grant ([perms, scopeteam, pkg], opts) {
    if (!perms || (perms !== 'read-only' && perms !== 'read-write')) {
      throw this.usageError('First argument must be either `read-only` or `read-write`.')
    }

    if (!scopeteam) {
      throw this.usageError('`<scope:team>` argument is required.')
    }

    const [, scope, team] = scopeteam.match(/^@?([^:]+):(.*)$/) || []

    if (!scope && !team) {
      throw this.usageError(
        'Second argument used incorrect format.\n' +
        'Example: @example:developers'
      )
    }

    return this.modifyPackage(pkg, opts, (pkgName, opts) =>
      libaccess.grant(pkgName, scopeteam, perms, opts), false)
  }

  async revoke ([scopeteam, pkg], opts) {
    if (!scopeteam) {
      throw this.usageError('`<scope:team>` argument is required.')
    }

    const [, scope, team] = scopeteam.match(/^@?([^:]+):(.*)$/) || []

    if (!scope || !team) {
      throw this.usageError(
        'First argument used incorrect format.\n' +
        'Example: @example:developers'
      )
    }

    return this.modifyPackage(pkg, opts, (pkgName, opts) =>
      libaccess.revoke(pkgName, scopeteam, opts))
  }

  get ['2fa-required'] () {
    return this.tfaRequired
  }

  tfaRequired ([pkg], opts) {
    return this.modifyPackage(pkg, opts, libaccess.tfaRequired, false)
  }

  get ['2fa-not-required'] () {
    return this.tfaNotRequired
  }

  tfaNotRequired ([pkg], opts) {
    return this.modifyPackage(pkg, opts, libaccess.tfaNotRequired, false)
  }

  get ['ls-packages'] () {
    return this.lsPackages
  }

  async lsPackages ([owner], opts) {
    if (!owner) {
      owner = await getIdentity(this.npm, opts)
    }

    const pkgs = await libaccess.lsPackages(owner, opts)

    // TODO - print these out nicely (breaking change)
    this.npm.output(JSON.stringify(pkgs, null, 2))
  }

  get ['ls-collaborators'] () {
    return this.lsCollaborators
  }

  async lsCollaborators ([pkg, usr], opts) {
    const pkgName = await this.getPackage(pkg, false)
    const collabs = await libaccess.lsCollaborators(pkgName, usr, opts)

    // TODO - print these out nicely (breaking change)
    this.npm.output(JSON.stringify(collabs, null, 2))
  }

  async edit () {
    throw new Error('edit subcommand is not implemented yet')
  }

  modifyPackage (pkg, opts, fn, requireScope = true) {
    return this.getPackage(pkg, requireScope)
      .then(pkgName => otplease(this.npm, opts, opts => fn(pkgName, opts)))
  }

  async getPackage (name, requireScope) {
    if (name && name.trim()) {
      return name.trim()
    } else {
      try {
        const pkg = await readPackageJson(path.resolve(this.npm.prefix, 'package.json'))
        name = pkg.name
      } catch (err) {
        if (err.code === 'ENOENT') {
          throw new Error(
            'no package name passed to command and no package.json found'
          )
        } else {
          throw err
        }
      }

      if (requireScope && !name.match(/^@[^/]+\/.*$/)) {
        throw this.usageError('This command is only available for scoped packages.')
      } else {
        return name
      }
    }
  }
}

module.exports = Access
