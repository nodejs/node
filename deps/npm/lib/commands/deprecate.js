const fetch = require('npm-registry-fetch')
const otplease = require('../utils/otplease.js')
const npa = require('npm-package-arg')
const semver = require('semver')
const getIdentity = require('../utils/get-identity.js')
const libaccess = require('libnpmaccess')
const BaseCommand = require('../base-command.js')

class Deprecate extends BaseCommand {
  static description = 'Deprecate a version of a package'
  static name = 'deprecate'
  static usage = ['<package-spec> <message>']
  static params = [
    'registry',
    'otp',
  ]

  static ignoreImplicitWorkspace = false

  async completion (opts) {
    if (opts.conf.argv.remain.length > 1) {
      return []
    }

    const username = await getIdentity(this.npm, this.npm.flatOptions)
    const packages = await libaccess.lsPackages(username, this.npm.flatOptions)
    return Object.keys(packages)
      .filter((name) =>
        packages[name] === 'read-write' &&
        (opts.conf.argv.remain.length === 0 ||
          name.startsWith(opts.conf.argv.remain[0])))
  }

  async exec ([pkg, msg]) {
    // msg == null because '' is a valid value, it indicates undeprecate
    if (!pkg || msg == null) {
      throw this.usageError()
    }

    // fetch the data and make sure it exists.
    const p = npa(pkg)
    // npa makes the default spec "latest", but for deprecation
    // "*" is the appropriate default.
    const spec = p.rawSpec === '' ? '*' : p.fetchSpec

    if (semver.validRange(spec, true) === null) {
      throw new Error(`invalid version range: ${spec}`)
    }

    const uri = '/' + p.escapedName
    const packument = await fetch.json(uri, {
      ...this.npm.flatOptions,
      spec: p,
      query: { write: true },
    })

    Object.keys(packument.versions)
      .filter(v => semver.satisfies(v, spec, { includePrerelease: true }))
      .forEach(v => {
        packument.versions[v].deprecated = msg
      })

    return otplease(this.npm.flatOptions, opts => fetch(uri, {
      ...opts,
      spec: p,
      method: 'PUT',
      body: packument,
      ignoreBody: true,
    }))
  }
}

module.exports = Deprecate
