var npm = require('./npm.js')
var mapToRegistry = require('./utils/map-to-registry.js')
var npa = require('npm-package-arg')

module.exports = deprecate

deprecate.usage = 'npm deprecate <pkg>[@<version>] <message>'

deprecate.completion = function (opts, cb) {
  // first, get a list of remote packages this user owns.
  // once we have a user account, then don't complete anything.
  if (opts.conf.argv.remain.length > 2) return cb()
  // get the list of packages by user
  var path = '/-/by-user/'
  mapToRegistry(path, npm.config, function (er, uri, c) {
    if (er) return cb(er)

    if (!(c && c.username)) return cb()

    var params = {
      timeout: 60000,
      auth: c
    }
    npm.registry.get(uri + c.username, params, function (er, list) {
      if (er) return cb()
      console.error(list)
      return cb(null, list[c.username])
    })
  })
}

function deprecate (args, cb) {
  var pkg = args[0]
  var msg = args[1]
  if (msg === undefined) return cb('Usage: ' + deprecate.usage)

  // fetch the data and make sure it exists.
  var p = npa(pkg)

  // npa makes the default spec "latest", but for deprecation
  // "*" is the appropriate default.
  var spec = p.rawSpec === '' ? '*' : p.fetchSpec

  mapToRegistry(p.name, npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    var params = {
      version: spec,
      message: msg,
      auth: auth
    }
    npm.registry.deprecate(uri, params, cb)
  })
}
