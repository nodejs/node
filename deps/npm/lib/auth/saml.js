var ssoAuth = require('./sso')
var npm = require('../npm')

module.exports.login = function login () {
  npm.config.set('sso-type', 'saml')
  ssoAuth.login.apply(this, arguments)
}
