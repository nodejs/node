module.exports = docs

docs.usage = "npm docs <pkgname>"

docs.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  registry.get("/-/short", 60000, function (er, list) {
    return cb(null, list || [])
  })
}

var npm = require("./npm.js")
  , registry = npm.registry
  , log = require("npmlog")
  , opener = require("opener")

function docs (args, cb) {
  if (!args.length) return cb(docs.usage)
  var project = args[0]
  var npmName = project.split("@").shift()
  registry.get(project + "/latest", 3600, function (er, d) {
    if (er) {
      if (project.split("/").length !== 2) return cb(er)
      
      var url = "https://github.com/" + project + "#readme"
      return opener(url, { command: npm.config.get("browser") }, cb)
    }

    var homepage = d.homepage
      , repo = d.repository || d.repositories
      , url = homepage ? homepage
            : "https://npmjs.org/package/" + d.name
    opener(url, { command: npm.config.get("browser") }, cb)
  })
}
