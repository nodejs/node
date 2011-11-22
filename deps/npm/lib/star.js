
module.exports = star

var npm = require("./npm.js")
  , registry = require("./utils/npm-registry-client/index.js")
  , log = require("./utils/log.js")
  , asyncMap = require("slide").asyncMap
  , output = require("./utils/output.js")

star.usage = "npm star <package> [pkg, pkg, ...]\n"
           + "npm unstar <package> [pkg, pkg, ...]"

star.completion = function (opts, cb) {
  registry.get("/-/short", null, 60000, function (er, list) {
    return cb(null, list || [])
  })
}

function star (args, cb) {
  if (!args.length) return cb(star.usage)
  var s = npm.config.get("unicode") ? "\u2605 " : "(*)"
    , u = npm.config.get("unicode") ? "\u2606 " : "( )"
    , using = !(npm.command.match(/^un/))
  if (!using) s = u
  asyncMap(args, function (pkg, cb) {
    registry.star(pkg, using, function (er, data, raw, req) {
      if (!er) {
        output.write(s + " "+pkg, npm.config.get("outfd"))
        log.verbose(data, "back from star/unstar")
      }
      cb(er, data, raw, req)
    })
  }, cb)
}
