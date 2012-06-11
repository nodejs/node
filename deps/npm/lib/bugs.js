
module.exports = bugs

bugs.usage = "npm bugs <pkgname>"

var exec = require("./utils/exec.js")
  , npm = require("./npm.js")
  , registry = npm.registry
  , log = require("npmlog")

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
    if (bugs) {
      if (typeof bugs === "string") return open(bugs, cb)
      if (bugs.url) return open(bugs.url, cb)
    }
    if (repo) {
      if (Array.isArray(repo)) repo = repo.shift()
      if (repo.hasOwnProperty("url")) repo = repo.url
      log.verbose("repository", repo)
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
  var args = [url]
    , browser = npm.config.get("browser")

  if (process.platform === "win32" && browser === "start") {
    args = [ "/c", "start" ].concat(args)
    browser = "cmd"
  }

  if (!browser) {
    var er = ["the 'browser' config is not set.  Try doing this:"
             ,"    npm config set browser google-chrome"
             ,"or:"
             ,"    npm config set browser lynx"].join("\n")
    return cb(er)
  }

  exec(browser, args, process.env, false, function () {})
  cb()
}
