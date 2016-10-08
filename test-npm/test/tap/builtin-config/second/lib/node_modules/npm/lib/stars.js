module.exports = stars

stars.usage = 'npm stars [<user>]'

var npm = require('./npm.js')
var log = require('npmlog')
var mapToRegistry = require('./utils/map-to-registry.js')
var output = require('./utils/output.js')

function stars (args, cb) {
  npm.commands.whoami([], true, function (er, username) {
    var name = args.length === 1 ? args[0] : username

    if (er) {
      if (er.code === 'ENEEDAUTH' && !name) {
        var needAuth = new Error("'npm stars' on your own user account requires auth")
        needAuth.code = 'ENEEDAUTH'
        return cb(needAuth)
      }

      if (er.code !== 'ENEEDAUTH') return cb(er)
    }

    mapToRegistry('', npm.config, function (er, uri, auth) {
      if (er) return cb(er)

      var params = {
        username: name,
        auth: auth
      }
      npm.registry.stars(uri, params, showstars)
    })
  })

  function showstars (er, data) {
    if (er) return cb(er)

    if (data.rows.length === 0) {
      log.warn('stars', 'user has not starred any packages.')
    } else {
      data.rows.forEach(function (a) {
        output(a.value)
      })
    }
    cb()
  }
}
