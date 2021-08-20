const sso = require('./sso.js')

const login = (npm, opts) => {
  npm.config.set('sso-type', 'oauth')
  return sso(npm, opts)
}

module.exports = login
