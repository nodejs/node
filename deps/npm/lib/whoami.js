'use strict'

const BB = require('bluebird')

const npmConfig = require('./config/figgy-config.js')
const fetch = require('libnpm/fetch')
const figgyPudding = require('figgy-pudding')
const npm = require('./npm.js')
const output = require('./utils/output.js')

const WhoamiConfig = figgyPudding({
  json: {},
  registry: {}
})

module.exports = whoami

whoami.usage = 'npm whoami [--registry <registry>]\n(just prints username according to given registry)'

function whoami ([spec], silent, cb) {
  // FIXME: need tighter checking on this, but is a breaking change
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }
  const opts = WhoamiConfig(npmConfig())
  return BB.try(() => {
    // First, check if we have a user/pass-based auth
    const registry = opts.registry
    if (!registry) throw new Error('no default registry set')
    return npm.config.getCredentialsByURI(registry)
  }).then(({username, token}) => {
    if (username) {
      return username
    } else if (token) {
      return fetch.json('/-/whoami', opts.concat({
        spec
      })).then(({username}) => {
        if (username) {
          return username
        } else {
          throw Object.assign(new Error(
            'Your auth token is no longer valid. Please log in again.'
          ), {code: 'ENEEDAUTH'})
        }
      })
    } else {
      // At this point, if they have a credentials object, it doesn't have a
      // token or auth in it.  Probably just the default registry.
      throw Object.assign(new Error(
        'This command requires you to be logged in.'
      ), {code: 'ENEEDAUTH'})
    }
  }).then(username => {
    if (silent) {
    } else if (opts.json) {
      output(JSON.stringify(username))
    } else {
      output(username)
    }
    return username
  }).nodeify(cb)
}
