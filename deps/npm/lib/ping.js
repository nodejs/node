var npm = require('./npm.js')
var output = require('./utils/output.js')

module.exports = ping

ping.usage = 'npm ping\nping registry'

function ping (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }
  var registry = npm.config.get('registry')
  if (!registry) return cb(new Error('no default registry set'))
  var auth = npm.config.getCredentialsByURI(registry)

  npm.registry.ping(registry, {auth: auth}, function (er, pong) {
    if (!silent) output(JSON.stringify(pong))
    cb(er, er ? null : pong)
  })
}
