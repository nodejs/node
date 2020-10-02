const sso = require('./sso.js')
const npm = require('../npm.js')

const login = (opts) => {
  npm.config.set('sso-type', 'oauth')
  return sso(opts)
}

module.exports = login
