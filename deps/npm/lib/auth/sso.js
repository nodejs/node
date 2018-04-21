var log = require('npmlog')
var npm = require('../npm.js')
var output = require('../utils/output')
var openUrl = require('../utils/open-url')

module.exports.login = function login (creds, registry, scope, cb) {
  var ssoType = npm.config.get('sso-type')
  if (!ssoType) { return cb(new Error('Missing option: sso-type')) }

  var params = {
    // We're reusing the legacy login endpoint, so we need some dummy
    // stuff here to pass validation. They're never used.
    auth: {
      username: 'npm_' + ssoType + '_auth_dummy_user',
      password: 'placeholder',
      email: 'support@npmjs.com',
      authType: ssoType
    }
  }
  npm.registry.adduser(registry, params, function (er, doc) {
    if (er) return cb(er)
    if (!doc || !doc.token) return cb(new Error('no SSO token returned'))
    if (!doc.sso) return cb(new Error('no SSO URL returned by services'))

    openUrl(doc.sso, 'to complete your login please visit', function () {
      pollForSession(registry, doc.token, function (err, username) {
        if (err) return cb(err)

        log.info('adduser', 'Authorized user %s', username)
        var scopeMessage = scope ? ' to scope ' + scope : ''
        output('Logged in as %s%s on %s.', username, scopeMessage, registry)

        cb(null, { token: doc.token })
      })
    })
  })
}

function pollForSession (registry, token, cb) {
  log.info('adduser', 'Polling for validated SSO session')
  npm.registry.whoami(registry, {
    auth: {
      token: token
    }
  }, function (er, username) {
    if (er && er.statusCode !== 401) {
      cb(er)
    } else if (!username) {
      setTimeout(function () {
        pollForSession(registry, token, cb)
      }, npm.config.get('sso-poll-frequency'))
    } else {
      cb(null, username)
    }
  })
}
