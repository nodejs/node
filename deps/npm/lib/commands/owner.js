const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const pacote = require('pacote')
const log = require('../utils/log-shim')
const otplease = require('../utils/otplease.js')
const readPackageJsonFast = require('read-package-json-fast')
const BaseCommand = require('../base-command.js')
const { resolve } = require('path')

const readJson = async (pkg) => {
  try {
    const json = await readPackageJsonFast(pkg)
    return json
  } catch {
    return {}
  }
}

class Owner extends BaseCommand {
  static description = 'Manage package owners'
  static name = 'owner'
  static params = [
    'registry',
    'otp',
  ]

  static usage = [
    'add <user> [<@scope>/]<pkg>',
    'rm <user> [<@scope>/]<pkg>',
    'ls [<@scope>/]<pkg>',
  ]

  static ignoreImplicitWorkspace = false

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length > 3) {
      return []
    }

    if (argv[1] !== 'owner') {
      argv.unshift('owner')
    }

    if (argv.length === 2) {
      return ['add', 'rm', 'ls']
    }

    // reaches registry in order to autocomplete rm
    if (argv[2] === 'rm') {
      if (this.npm.config.get('global')) {
        return []
      }
      const { name } = await readJson(resolve(this.npm.prefix, 'package.json'))
      if (!name) {
        return []
      }

      const spec = npa(name)
      const data = await pacote.packument(spec, {
        ...this.npm.flatOptions,
        fullMetadata: true,
      })
      if (data && data.maintainers && data.maintainers.length) {
        return data.maintainers.map(m => m.name)
      }
    }
    return []
  }

  async exec ([action, ...args]) {
    switch (action) {
      case 'ls':
      case 'list':
        return this.ls(args[0])
      case 'add':
        return this.changeOwners(args[0], args[1], 'add')
      case 'rm':
      case 'remove':
        return this.changeOwners(args[0], args[1], 'rm')
      default:
        throw this.usageError()
    }
  }

  async ls (pkg) {
    pkg = await this.getPkg(pkg)
    const spec = npa(pkg)

    try {
      const packumentOpts = { ...this.npm.flatOptions, fullMetadata: true, preferOnline: true }
      const { maintainers } = await pacote.packument(spec, packumentOpts)
      if (!maintainers || !maintainers.length) {
        this.npm.output('no admin found')
      } else {
        this.npm.output(maintainers.map(m => `${m.name} <${m.email}>`).join('\n'))
      }
    } catch (err) {
      log.error('owner ls', "Couldn't get owner data", pkg)
      throw err
    }
  }

  async getPkg (pkg) {
    if (!pkg) {
      if (this.npm.config.get('global')) {
        throw this.usageError()
      }
      const { name } = await readJson(resolve(this.npm.prefix, 'package.json'))
      if (!name) {
        throw this.usageError()
      }

      return name
    }
    return pkg
  }

  async changeOwners (user, pkg, addOrRm) {
    if (!user) {
      throw this.usageError()
    }

    pkg = await this.getPkg(pkg)
    log.verbose(`owner ${addOrRm}`, '%s to %s', user, pkg)

    const spec = npa(pkg)
    const uri = `/-/user/org.couchdb.user:${encodeURIComponent(user)}`
    let u

    try {
      u = await npmFetch.json(uri, this.npm.flatOptions)
    } catch (err) {
      log.error('owner mutate', `Error getting user data for ${user}`)
      throw err
    }

    // normalize user data
    u = { name: u.name, email: u.email }

    const data = await pacote.packument(spec, {
      ...this.npm.flatOptions,
      fullMetadata: true,
      preferOnline: true,
    })

    const owners = data.maintainers || []
    let maintainers
    if (addOrRm === 'add') {
      const existing = owners.find(o => o.name === u.name)
      if (existing) {
        log.info(
          'owner add',
          `Already a package owner: ${existing.name} <${existing.email}>`
        )
        return
      }
      maintainers = [
        ...owners,
        u,
      ]
    } else {
      maintainers = owners.filter(o => o.name !== u.name)

      if (maintainers.length === owners.length) {
        log.info('owner rm', 'Not a package owner: ' + u.name)
        return false
      }

      if (!maintainers.length) {
        throw Object.assign(
          new Error(
            'Cannot remove all owners of a package. Add someone else first.'
          ),
          { code: 'EOWNERRM' }
        )
      }
    }

    const dataPath = `/${spec.escapedName}/-rev/${encodeURIComponent(data._rev)}`
    try {
      const res = await otplease(this.npm.flatOptions, opts => {
        return npmFetch.json(dataPath, {
          ...opts,
          method: 'PUT',
          body: {
            _id: data._id,
            _rev: data._rev,
            maintainers,
          },
          spec,
        })
      })
      if (addOrRm === 'add') {
        this.npm.output(`+ ${user} (${spec.name})`)
      } else {
        this.npm.output(`- ${user} (${spec.name})`)
      }
      return res
    } catch (err) {
      throw Object.assign(
        new Error('Failed to update package: ' + JSON.stringify(err.message)),
        { code: 'EOWNERMUTATE' }
      )
    }
  }
}

module.exports = Owner
