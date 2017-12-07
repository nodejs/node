'use strict'
const read = require('../utils/read-user-info.js')
const profile = require('npm-profile')
const log = require('npmlog')
const npm = require('../npm.js')
const output = require('../utils/output.js')
const pacoteOpts = require('../config/pacote')
const fetchOpts = require('../config/fetch-opts')

module.exports.login = function login (creds, registry, scope, cb) {
  let username = creds.username || ''
  let password = creds.password || ''
  let email = creds.email || ''
  const auth = {}
  if (npm.config.get('otp')) auth.otp = npm.config.get('otp')

  return read.username('Username:', username, {log: log}).then((u) => {
    username = u
    return read.password('Password: ', password)
  }).then((p) => {
    password = p
    return read.email('Email: (this IS public) ', email, {log: log})
  }).then((e) => {
    email = e
    return profile.login(username, password, {registry: registry, auth: auth}).catch((err) => {
      if (err.code === 'EOTP') throw err
      return profile.adduser(username, email, password, {
        registry: registry,
        opts: fetchOpts.fromPacote(pacoteOpts())
      })
    }).catch((err) => {
      if (err.code === 'EOTP' && !auth.otp) {
        return read.otp('Authenticator provided OTP:').then((otp) => {
          auth.otp = otp
          return profile.login(username, password, {registry: registry, auth: auth})
        })
      } else {
        throw err
      }
    })
  }).then((result) => {
    const newCreds = {}
    if (result && result.token) {
      newCreds.token = result.token
    } else {
      newCreds.username = username
      newCreds.password = password
      newCreds.email = email
      newCreds.alwaysAuth = npm.config.get('always-auth')
    }

    log.info('adduser', 'Authorized user %s', username)
    const scopeMessage = scope ? ' to scope ' + scope : ''
    output('Logged in as %s%s on %s.', username, scopeMessage, registry)
    cb(null, newCreds)
  }).catch(cb)
}
