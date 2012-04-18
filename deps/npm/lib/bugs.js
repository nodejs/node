
module.exports = bugs

bugs.usage = "npm bugs <pkgname>"

bugs.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  registry.get("/-/short", null, 60000, function (er, list) {
    return cb(null, list || [])
  })
}

var exec = require("./utils/exec.js")
  , registry = require("./utils/npm-registry-client/index.js")
  , npm = require("./npm.js")
  , log = require("./utils/log.js")

function bugs (args, cb) {
  if (!args.length) return cb(bugs.usage)
  var n = args[0].split("@").shift()
  registry.get(n, "latest", 3600, function (er, d) {
    if (er) return cb(er)
    var bugs = d.bugs
      , repo = d.repository || d.repositories
    if (bugs) {
      if (typeof bugs === "string") return open(bugs, cb)
      if (bugs.url) return open(bugs.url, cb)
    }
    if (repo) {
      if (Array.isArray(repo)) repo = repo.shift()
      if (repo.hasOwnProperty("url")) repo = repo.url
      log.verbose(repo, "repository")
      if (repo && repo.match(/^(https?:\/\/|git(:\/\/|@))github.com/)) {
        return open(repo.replace(/^git(@|:\/\/)/, "http://")
                        .replace(/^https?:\/\/github.com:/, "github.com/")
                        .replace(/\.git$/, '')+"/issues", cb)
      }
    }
    return open("http://search.npmjs.org/#/" + d.name, cb)
  })
}

function open (url, cb) {
  exec(npm.config.get("browser"), [url], log.er(cb,
    "Failed to open "+url+" in a browser.  It could be that the\n"+
    "'browser' config is not set.  Try doing this:\n"+
    "    npm config set browser google-chrome\n"+
    "or:\n"+
    "    npm config set browser lynx\n"))
}
