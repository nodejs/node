const log = require('npmlog')
const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const pacote = require('pacote')

const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const readLocalPkg = require('./utils/read-local-package.js')
const usageUtil = require('./utils/usage.js')

class Owner {
  constructor (npm) {
    this.npm = npm
  }

  get usage () {
    return usageUtil(
      'owner',
      'npm owner add <user> [<@scope>/]<pkg>' +
      '\nnpm owner rm <user> [<@scope>/]<pkg>' +
      '\nnpm owner ls [<@scope>/]<pkg>'
    )
  }

  get usageError () {
    return Object.assign(new Error(this.usage), { code: 'EUSAGE' })
  }

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length > 3)
      return []

    if (argv[1] !== 'owner')
      argv.unshift('owner')

    if (argv.length === 2)
      return ['add', 'rm', 'ls']

    // reaches registry in order to autocomplete rm
    if (argv[2] === 'rm') {
      const pkgName = await readLocalPkg(this.npm)
      if (!pkgName)
        return []

      const spec = npa(pkgName)
      const data = await pacote.packument(spec, {
        ...this.npm.flatOptions,
        fullMetadata: true,
      })
      if (data && data.maintainers && data.maintainers.length)
        return data.maintainers.map(m => m.name)
    }
    return []
  }

  exec (args, cb) {
    this.owner(args).then(() => cb()).catch(cb)
  }

  async owner ([action, ...args]) {
    const opts = this.npm.flatOptions
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
        throw this.usageError
    }
  }

  async ls (pkg, opts) {
    if (!pkg) {
      const pkgName = await readLocalPkg(this.npm)
      if (!pkgName)
        throw this.usageError

      pkg = pkgName
    }

    const spec = npa(pkg)

    try {
      const packumentOpts = { ...opts, fullMetadata: true }
      const { maintainers } = await pacote.packument(spec, packumentOpts)
      if (!maintainers || !maintainers.length)
        output('no admin found')
      else
        output(maintainers.map(o => `${o.name} <${o.email}>`).join('\n'))

      return maintainers
    } catch (err) {
      log.error('owner ls', "Couldn't get owner data", pkg)
      throw err
    }
  }

  async add (user, pkg, opts) {
    if (!user)
      throw this.usageError

    if (!pkg) {
      const pkgName = await readLocalPkg(this.npm)
      if (!pkgName)
        throw this.usageError

      pkg = pkgName
    }
    log.verbose('owner add', '%s to %s', user, pkg)

    const spec = npa(pkg)
    return putOwners(spec, user, opts, validateAddOwner)
  }

  async rm (user, pkg, opts) {
    if (!user)
      throw this.usageError

    if (!pkg) {
      const pkgName = await readLocalPkg(this.npm)
      if (!pkgName)
        throw this.usageError

      pkg = pkgName
    }
    log.verbose('owner rm', '%s from %s', user, pkg)

    const spec = npa(pkg)
    return putOwners(spec, user, opts, validateRmOwner)
  }
}
module.exports = Owner

const validateAddOwner = (newOwner, owners) => {
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

const validateRmOwner = (rmOwner, owners) => {
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

const putOwners = async (spec, user, opts, validation) => {
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
  if (!m)
    return // invalid owners

  const body = {
    _id: data._id,
    _rev: data._rev,
    maintainers: m,
  }
  const dataPath = `/${spec.escapedName}/-rev/${encodeURIComponent(data._rev)}`
  const res = await otplease(opts, opts =>
    npmFetch.json(dataPath, {
      ...opts,
      method: 'PUT',
      body,
      spec,
    }))

  if (!res.error) {
    if (m.length < before)
      output(`- ${user} (${spec.name})`)
    else
      output(`+ ${user} (${spec.name})`)
  } else {
    throw Object.assign(
      new Error('Failed to update package: ' + JSON.stringify(res)),
      { code: 'EOWNERMUTATE' }
    )
  }
  return res
}
