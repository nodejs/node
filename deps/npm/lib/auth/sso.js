'use strict'

const BB = require('bluebird')

const figgyPudding = require('figgy-pudding')
const log = require('npmlog')
const npmConfig = require('../config/figgy-config.js')
const npmFetch = require('npm-registry-fetch')
const output = require('../utils/output.js')
const openUrl = BB.promisify(require('../utils/open-url.js'))
const otplease = require('../utils/otplease.js')
const profile = require('libnpm/profile')

const SsoOpts = figgyPudding({
  ssoType: 'sso-type',
  'sso-type': {},
  ssoPollFrequency: 'sso-poll-frequency',
  'sso-poll-frequency': {}
})

module.exports.login = function login (creds, registry, scope, cb) {
  const opts = SsoOpts(npmConfig()).concat({creds, registry, scope})
  const ssoType = opts.ssoType
  if (!ssoType) { return cb(new Error('Missing option: sso-type')) }

  // We're reusing the legacy login endpoint, so we need some dummy
  // stuff here to pass validation. They're never used.
  const auth = {
    username: 'npm_' + ssoType + '_auth_dummy_user',
    password: 'placeholder',
    email: 'support@npmjs.com',
    authType: ssoType
  }

  otplease(opts,
    opts => profile.loginCouch(auth.username, auth.password, opts)
  ).then(({token, sso}) => {
    if (!token) { throw new Error('no SSO token returned') }
    if (!sso) { throw new Error('no SSO URL returned by services') }
    return openUrl(sso, 'to complete your login please visit').then(() => {
      return pollForSession(registry, token, opts)
    }).then(username => {
      log.info('adduser', 'Authorized user %s', username)
      var scopeMessage = scope ? ' to scope ' + scope : ''
      output('Logged in as %s%s on %s.', username, scopeMessage, registry)
      return {token}
    })
  }).nodeify(cb)
}

function pollForSession (registry, token, opts) {
  log.info('adduser', 'Polling for validated SSO session')
  return npmFetch.json(
    '/-/whoami', opts.concat({registry, forceAuth: {token}})
  ).then(
    ({username}) => username,
    err => {
      if (err.code === 'E401') {
        return sleep(opts['sso-poll-frequency']).then(() => {
          return pollForSession(registry, token, opts)
        })
      } else {
        throw err
      }
    }
  )
}

function sleep (time) {
  return new BB((resolve) => {
    setTimeout(resolve, time)
  })
}
