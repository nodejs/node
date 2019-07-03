module.exports = adduser

const log = require('npmlog')
const npm = require('./npm.js')
const usage = require('./utils/usage')
let crypto

try {
  crypto = require('crypto')
} catch (ex) {}

adduser.usage = usage(
  'adduser',
  'npm adduser [--registry=url] [--scope=@orgname] [--auth-type=legacy] [--always-auth]'
)

function adduser (args, cb) {
  if (!crypto) {
    return cb(new Error(
      'You must compile node with ssl support to use the adduser feature'
    ))
  }

  let registry = npm.config.get('registry')
  const scope = npm.config.get('scope')
  const creds = npm.config.getCredentialsByURI(npm.config.get('registry'))

  if (scope) {
    const scopedRegistry = npm.config.get(scope + ':registry')
    const cliRegistry = npm.config.get('registry', 'cli')
    if (scopedRegistry && !cliRegistry) registry = scopedRegistry
  }

  log.disableProgress()

  let auth
  try {
    auth = require('./auth/' + npm.config.get('auth-type'))
  } catch (e) {
    return cb(new Error('no such auth module'))
  }
  auth.login(creds, registry, scope, function (err, newCreds) {
    if (err) return cb(err)

    npm.config.del('_token', 'user') // prevent legacy pollution
    if (scope) npm.config.set(scope + ':registry', registry, 'user')
    npm.config.setCredentialsByURI(registry, newCreds)
    npm.config.save('user', cb)
  })
}
