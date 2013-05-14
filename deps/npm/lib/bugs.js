
module.exports = bugs

bugs.usage = "npm bugs <pkgname>"

var npm = require("./npm.js")
  , registry = npm.registry
  , log = require("npmlog")
  , opener = require("opener")

bugs.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  registry.get("/-/short", 60000, function (er, list) {
    return cb(null, list || [])
  })
}

function bugs (args, cb) {
  if (!args.length) return cb(bugs.usage)
  var n = args[0].split("@").shift()
  registry.get(n + "/latest", 3600, function (er, d) {
    if (er) return cb(er)
    var bugs = d.bugs
      , repo = d.repository || d.repositories
      , url
    if (bugs) {
      url = (typeof bugs === "string") ? bugs : bugs.url
    } else if (repo) {
      if (Array.isArray(repo)) repo = repo.shift()
      if (repo.hasOwnProperty("url")) repo = repo.url
      log.verbose("repository", repo)
      if (repo && repo.match(/^(https?:\/\/|git(:\/\/|@))github.com/)) {
        url = repo.replace(/^git(@|:\/\/)/, "https://")
                  .replace(/^https?:\/\/github.com:/, "https://github.com/")
                  .replace(/\.git$/, '')+"/issues"
      }
    }
    if (!url) {
      url = "https://npmjs.org/package/" + d.name
    }
    opener(url, { command: npm.config.get("browser") }, cb)
  })
}
