module.exports = stars

stars.usage = "npm stars [username]"

var npm = require("./npm.js")
  , log = require("npmlog")
  , mapToRegistry = require("./utils/map-to-registry.js")

function stars (args, cb) {
  npm.commands.whoami([], true, function (er, username) {
    var name = args.length === 1 ? args[0] : username
    mapToRegistry("", npm.config, function (er, uri, auth) {
      if (er) return cb(er)

      var params = {
        username : name,
        auth     : auth
      }
      npm.registry.stars(uri, params, showstars)
    })
  })

  function showstars (er, data) {
    if (er) return cb(er)

    if (data.rows.length === 0) {
      log.warn("stars", "user has not starred any packages.")
    } else {
      data.rows.forEach(function(a) {
        console.log(a.value)
      })
    }
    cb()
  }
}
