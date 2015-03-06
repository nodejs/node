
module.exports = star

var npm = require("./npm.js")
  , log = require("npmlog")
  , asyncMap = require("slide").asyncMap
  , mapToRegistry = require("./utils/map-to-registry.js")

star.usage = "npm star <package> [pkg, pkg, ...]\n"
           + "npm unstar <package> [pkg, pkg, ...]"

star.completion = function (opts, cb) {
  // FIXME: there used to be registry completion here, but it stopped making
  // sense somewhere around 50,000 packages on the registry
  cb()
}

function star (args, cb) {
  if (!args.length) return cb(star.usage)
  var s = npm.config.get("unicode") ? "\u2605 " : "(*)"
    , u = npm.config.get("unicode") ? "\u2606 " : "( )"
    , using = !(npm.command.match(/^un/))
  if (!using) s = u
  asyncMap(args, function (pkg, cb) {
    mapToRegistry(pkg, npm.config, function (er, uri, auth) {
      if (er) return cb(er)

      var params = {
        starred : using,
        auth    : auth
      }
      npm.registry.star(uri, params, function (er, data, raw, req) {
        if (!er) {
          console.log(s + " "+pkg)
          log.verbose("star", data)
        }
        cb(er, data, raw, req)
      })
    })
  }, cb)
}
