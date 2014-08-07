module.exports = stars

stars.usage = "npm stars [username]"

var url = require("url")
  , npm = require("./npm.js")
  , registry = npm.registry
  , log = require("npmlog")

function stars (args, cb) {
  var name = args.length === 1 ? args[0] : npm.config.get("username")
    , uri = url.resolve(npm.config.get("registry"), name)
  registry.stars(uri, showstars)

  function showstars (er, data) {
    if (er) {
      return cb(er)
    }

    if (data.rows.length === 0) {
      log.warn('stars', 'user has not starred any packages.')
    } else {
      data.rows.forEach(function(a) {
        console.log(a.value)
      })
    }
    cb()
  }
}
