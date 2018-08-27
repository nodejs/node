'use strict'
const read = require('../utils/read-user-info.js')
const profile = require('npm-profile')
const log = require('npmlog')
const npm = require('../npm.js')
const output = require('../utils/output.js')
const pacoteOpts = require('../config/pacote')
const fetchOpts = require('../config/fetch-opts')
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

module.exports.login = (creds, registry, scope, cb) => {
  const conf = {
    log: log,
    creds: creds,
    registry: registry,
    auth: {
      otp: npm.config.get('otp')
    },
    scope: scope,
    opts: fetchOpts.fromPacote(pacoteOpts())
  }
  login(conf).then((newCreds) => cb(null, newCreds)).catch(cb)
}

function login (conf) {
  return profile.login(openerPromise, loginPrompter, conf)
    .catch((err) => {
      if (err.code === 'EOTP') throw err
      const u = conf.creds.username
      const p = conf.creds.password
      const e = conf.creds.email
      if (!(u && p && e)) throw err
      return profile.adduserCouch(u, e, p, conf)
    })
    .catch((err) => {
      if (err.code !== 'EOTP') throw err
      return read.otp('Authenticator provided OTP:').then((otp) => {
        conf.auth.otp = otp
        const u = conf.creds.username
        const p = conf.creds.password
        return profile.loginCouch(u, p, conf)
      })
    }).then((result) => {
      const newCreds = {}
      if (result && result.token) {
        newCreds.token = result.token
      } else {
        newCreds.username = conf.creds.username
        newCreds.password = conf.creds.password
        newCreds.email = conf.creds.email
        newCreds.alwaysAuth = npm.config.get('always-auth')
      }

      const usermsg = conf.creds.username ? ' user ' + conf.creds.username : ''
      conf.log.info('login', 'Authorized' + usermsg)
      const scopeMessage = conf.scope ? ' to scope ' + conf.scope : ''
      const userout = conf.creds.username ? ' as ' + conf.creds.username : ''
      output('Logged in%s%s on %s.', userout, scopeMessage, conf.registry)
      return newCreds
    })
}
