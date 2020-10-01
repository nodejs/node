'use strict'

const npm = require('./npm.js')
const fetch = require('npm-registry-fetch')
const otplease = require('./utils/otplease.js')
const npa = require('npm-package-arg')
const semver = require('semver')
const getItentity = require('./utils/get-identity')

module.exports = deprecate

deprecate.usage = 'npm deprecate <pkg>[@<version>] <message>'

deprecate.completion = function (opts, cb) {
  return Promise.resolve().then(() => {
    if (opts.conf.argv.remain.length > 2) { return }
    return getItentity(npm.flatOptions).then(username => {
      if (username) {
        // first, get a list of remote packages this user owns.
        // once we have a user account, then don't complete anything.
        // get the list of packages by user
        return fetch(
          `/-/by-user/${encodeURIComponent(username)}`,
          npm.flatOptions
        ).then(list => list[username])
      }
    })
  }).then(() => cb(), er => cb(er))
}

function deprecate ([pkg, msg], opts, cb) {
  if (typeof cb !== 'function') {
    cb = opts
    opts = null
  }
  opts = opts || npm.flatOptions
  return Promise.resolve().then(() => {
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
    return fetch.json(uri, {
      ...opts,
      spec: p,
      query: { write: true }
    }).then(packument => {
      // filter all the versions that match
      Object.keys(packument.versions)
        .filter(v => semver.satisfies(v, spec))
        .forEach(v => { packument.versions[v].deprecated = msg })
      return otplease(opts, opts => fetch(uri, {
        ...opts,
        spec: p,
        method: 'PUT',
        body: packument,
        ignoreBody: true
      }))
    })
  }).then(() => cb(), cb)
}
