'use strict'

const read = require('../utils/read-user-info.js')
const profile = require('libnpm/profile')
const log = require('npmlog')
const figgyPudding = require('figgy-pudding')
const npmConfig = require('../config/figgy-config.js')
const output = require('../utils/output.js')
const openUrl = require('../utils/open-url')

const openerPromise = (url) => new Promise((resolve, reject) => {
  openUrl(url, 'to complete your login please visit', (er) => er ? reject(er) : resolve())
})

const loginPrompter = (creds) => {
  const opts = { log: log }
  return read.username('Username:', creds.username, opts).then((u) => {
    creds.username = u
    return read.password('Password:', creds.password)
  }).then((p) => {
    creds.password = p
    return read.email('Email: (this IS public) ', creds.email, opts)
  }).then((e) => {
    creds.email = e
    return creds
  })
}

const LoginOpts = figgyPudding({
  'always-auth': {},
  creds: {},
  log: {default: () => log},
  registry: {},
  scope: {}
})

module.exports.login = (creds = {}, registry, scope, cb) => {
  const opts = LoginOpts(npmConfig()).concat({scope, registry, creds})
  login(opts).then((newCreds) => cb(null, newCreds)).catch(cb)
}

function login (opts) {
  return profile.login(openerPromise, loginPrompter, opts)
    .catch((err) => {
      if (err.code === 'EOTP') throw err
      const u = opts.creds.username
      const p = opts.creds.password
      const e = opts.creds.email
      if (!(u && p && e)) throw err
      return profile.adduserCouch(u, e, p, opts)
    })
    .catch((err) => {
      if (err.code !== 'EOTP') throw err
      return read.otp(
        'Enter one-time password: '
      ).then(otp => {
        const u = opts.creds.username
        const p = opts.creds.password
        return profile.loginCouch(u, p, opts.concat({otp}))
      })
    }).then((result) => {
      const newCreds = {}
      if (result && result.token) {
        newCreds.token = result.token
      } else {
        newCreds.username = opts.creds.username
        newCreds.password = opts.creds.password
        newCreds.email = opts.creds.email
        newCreds.alwaysAuth = opts['always-auth']
      }

      const usermsg = opts.creds.username ? ' user ' + opts.creds.username : ''
      opts.log.info('login', 'Authorized' + usermsg)
      const scopeMessage = opts.scope ? ' to scope ' + opts.scope : ''
      const userout = opts.creds.username ? ' as ' + opts.creds.username : ''
      output('Logged in%s%s on %s.', userout, scopeMessage, opts.registry)
      return newCreds
    })
}
