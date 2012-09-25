
module.exports = docs

docs.usage = "npm docs <pkgname>"

docs.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  registry.get("/-/short", 60000, function (er, list) {
    return cb(null, list || [])
  })
}

var exec = require("./utils/exec.js")
  , npm = require("./npm.js")
  , registry = npm.registry
  , log = require("npmlog")
  , opener = require("opener")

function docs (args, cb) {
  if (!args.length) return cb(docs.usage)
  var n = args[0].split("@").shift()
  registry.get(n + "/latest", 3600, function (er, d) {
    if (er) return cb(er)
    var homepage = d.homepage
      , repo = d.repository || d.repositories
      , url = homepage ? homepage
            : "https://npmjs.org/package/" + d.name
    opener(url, { command: npm.config.get("browser") }, cb)
  })
}
