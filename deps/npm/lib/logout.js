'use strict'

const BB = require('bluebird')

const eu = encodeURIComponent
const getAuth = require('npm-registry-fetch/auth.js')
const log = require('npmlog')
const npm = require('./npm.js')
const npmConfig = require('./config/figgy-config.js')
const npmFetch = require('libnpm/fetch')

logout.usage = 'npm logout [--registry=<url>] [--scope=<@scope>]'

function afterLogout (normalized) {
  var scope = npm.config.get('scope')

  if (scope) npm.config.del(scope + ':registry')

  npm.config.clearCredentialsByURI(normalized)
  return BB.fromNode(cb => npm.config.save('user', cb))
}

module.exports = logout
function logout (args, cb) {
  const opts = npmConfig()
  BB.try(() => {
    const reg = npmFetch.pickRegistry('foo', opts)
    const auth = getAuth(reg, opts)
    if (auth.token) {
      log.verbose('logout', 'clearing session token for', reg)
      return npmFetch(`/-/user/token/${eu(auth.token)}`, opts.concat({
        method: 'DELETE',
        ignoreBody: true
      })).then(() => afterLogout(reg))
    } else if (auth.username || auth.password) {
      log.verbose('logout', 'clearing user credentials for', reg)
      return afterLogout(reg)
    } else {
      throw new Error(
        'Not logged in to', reg + ',', "so can't log out."
      )
    }
  }).nodeify(cb)
}
