'use strict'

const BB = require('bluebird')

const npmConfig = require('./config/figgy-config.js')
const fetch = require('libnpm/fetch')
const figgyPudding = require('figgy-pudding')
const otplease = require('./utils/otplease.js')
const npa = require('libnpm/parse-arg')
const semver = require('semver')
const whoami = require('./whoami.js')

const DeprecateConfig = figgyPudding({})

module.exports = deprecate

deprecate.usage = 'npm deprecate <pkg>[@<version>] <message>'

deprecate.completion = function (opts, cb) {
  return BB.try(() => {
    if (opts.conf.argv.remain.length > 2) { return }
    return whoami([], true, () => {}).then(username => {
      if (username) {
        // first, get a list of remote packages this user owns.
        // once we have a user account, then don't complete anything.
        // get the list of packages by user
        return fetch(
          `/-/by-user/${encodeURIComponent(username)}`,
          DeprecateConfig()
        ).then(list => list[username])
      }
    })
  }).nodeify(cb)
}

function deprecate ([pkg, msg], opts, cb) {
  if (typeof cb !== 'function') {
    cb = opts
    opts = null
  }
  opts = DeprecateConfig(opts || npmConfig())
  return BB.try(() => {
    if (msg == null) throw new Error(`Usage: ${deprecate.usage}`)
    // fetch the data and make sure it exists.
    const p = npa(pkg)

    // npa makes the default spec "latest", but for deprecation
    // "*" is the appropriate default.
    const spec = p.rawSpec === '' ? '*' : p.fetchSpec

    if (semver.validRange(spec, true) === null) {
      throw new Error('invalid version range: ' + spec)
    }

    const uri = '/' + p.escapedName
    return fetch.json(uri, opts.concat({
      spec: p,
      query: {write: true}
    })).then(packument => {
      // filter all the versions that match
      Object.keys(packument.versions)
        .filter(v => semver.satisfies(v, spec))
        .forEach(v => { packument.versions[v].deprecated = msg })
      return otplease(opts, opts => fetch(uri, opts.concat({
        spec: p,
        method: 'PUT',
        body: packument,
        ignoreBody: true
      })))
    })
  }).nodeify(cb)
}
