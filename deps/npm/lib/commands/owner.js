const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const pacote = require('pacote')
const log = require('../utils/log-shim')
const otplease = require('../utils/otplease.js')
const readLocalPkgName = require('../utils/read-package-name.js')
const BaseCommand = require('../base-command.js')

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
      const pkgName = await readLocalPkgName(this.npm.prefix)
      if (!pkgName) {
        return []
      }

      const spec = npa(pkgName)
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
    const opts = {
      ...this.npm.flatOptions,
    }
    switch (action) {
      case 'ls':
      case 'list':
        return this.ls(args[0], opts)
      case 'add':
        return this.add(args[0], args[1], opts)
      case 'rm':
      case 'remove':
        return this.rm(args[0], args[1], opts)
      default:
        throw this.usageError()
    }
  }

  async ls (pkg, opts) {
    if (!pkg) {
      if (this.npm.config.get('global')) {
        throw this.usageError()
      }

      const pkgName = await readLocalPkgName(this.npm.prefix)
      if (!pkgName) {
        throw this.usageError()
      }

      pkg = pkgName
    }

    const spec = npa(pkg)

    try {
      const packumentOpts = { ...opts, fullMetadata: true }
      const { maintainers } = await pacote.packument(spec, packumentOpts)
      if (!maintainers || !maintainers.length) {
        this.npm.output('no admin found')
      } else {
        this.npm.output(maintainers.map(o => `${o.name} <${o.email}>`).join('\n'))
      }

      return maintainers
    } catch (err) {
      log.error('owner ls', "Couldn't get owner data", pkg)
      throw err
    }
  }

  async add (user, pkg, opts) {
    if (!user) {
      throw this.usageError()
    }

    if (!pkg) {
      if (this.npm.config.get('global')) {
        throw this.usageError()
      }
      const pkgName = await readLocalPkgName(this.npm.prefix)
      if (!pkgName) {
        throw this.usageError()
      }

      pkg = pkgName
    }
    log.verbose('owner add', '%s to %s', user, pkg)

    const spec = npa(pkg)
    return this.putOwners(spec, user, opts,
      (newOwner, owners) => this.validateAddOwner(newOwner, owners))
  }

  async rm (user, pkg, opts) {
    if (!user) {
      throw this.usageError()
    }

    if (!pkg) {
      if (this.npm.config.get('global')) {
        throw this.usageError()
      }
      const pkgName = await readLocalPkgName(this.npm.prefix)
      if (!pkgName) {
        throw this.usageError()
      }

      pkg = pkgName
    }
    log.verbose('owner rm', '%s from %s', user, pkg)

    const spec = npa(pkg)
    return this.putOwners(spec, user, opts,
      (rmOwner, owners) => this.validateRmOwner(rmOwner, owners))
  }

  async putOwners (spec, user, opts, validation) {
    const uri = `/-/user/org.couchdb.user:${encodeURIComponent(user)}`
    let u = ''

    try {
      u = await npmFetch.json(uri, opts)
    } catch (err) {
      log.error('owner mutate', `Error getting user data for ${user}`)
      throw err
    }

    if (user && (!u || !u.name || u.error)) {
      throw Object.assign(
        new Error(
          "Couldn't get user data for " + user + ': ' + JSON.stringify(u)
        ),
        { code: 'EOWNERUSER' }
      )
    }

    // normalize user data
    u = { name: u.name, email: u.email }

    const data = await pacote.packument(spec, { ...opts, fullMetadata: true })

    // save the number of maintainers before validation for comparison
    const before = data.maintainers ? data.maintainers.length : 0

    const m = validation(u, data.maintainers)
    if (!m) {
      return
    } // invalid owners

    const body = {
      _id: data._id,
      _rev: data._rev,
      maintainers: m,
    }
    const dataPath = `/${spec.escapedName}/-rev/${encodeURIComponent(data._rev)}`
    const res = await otplease(opts, opts => {
      return npmFetch.json(dataPath, {
        ...opts,
        method: 'PUT',
        body,
        spec,
      })
    })

    if (!res.error) {
      if (m.length < before) {
        this.npm.output(`- ${user} (${spec.name})`)
      } else {
        this.npm.output(`+ ${user} (${spec.name})`)
      }
    } else {
      throw Object.assign(
        new Error('Failed to update package: ' + JSON.stringify(res)),
        { code: 'EOWNERMUTATE' }
      )
    }
    return res
  }

  validateAddOwner (newOwner, owners) {
    owners = owners || []
    for (const o of owners) {
      if (o.name === newOwner.name) {
        log.info(
          'owner add',
          'Already a package owner: ' + o.name + ' <' + o.email + '>'
        )
        return false
      }
    }
    return [
      ...owners,
      newOwner,
    ]
  }

  validateRmOwner (rmOwner, owners) {
    let found = false
    const m = owners.filter(function (o) {
      var match = (o.name === rmOwner.name)
      found = found || match
      return !match
    })

    if (!found) {
      log.info('owner rm', 'Not a package owner: ' + rmOwner.name)
      return false
    }

    if (!m.length) {
      throw Object.assign(
        new Error(
          'Cannot remove all owners of a package. Add someone else first.'
        ),
        { code: 'EOWNERRM' }
      )
    }

    return m
  }
}
module.exports = Owner
