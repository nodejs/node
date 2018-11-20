var ssoAuth = require('./sso')
var npm = require('../npm')

module.exports.login = function login () {
  npm.config.set('sso-type', 'oauth')
  ssoAuth.login.apply(this, arguments)
}
