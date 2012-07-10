
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

function docs (args, cb) {
  if (!args.length) return cb(docs.usage)
  var n = args[0].split("@").shift()
  registry.get(n + "/latest", 3600, function (er, d) {
    if (er) return cb(er)
    var homepage = d.homepage
      , repo = d.repository || d.repositories
    if (homepage) return open(homepage, cb)
    if (repo) {
      if (Array.isArray(repo)) repo = repo.shift()
      if (repo.hasOwnProperty("url")) repo = repo.url
      log.verbose("repository", repo)
      if (repo) {
        return open(repo.replace(/^git(@|:\/\/)/, 'http://')
                        .replace(/\.git$/, '')+"#readme", cb)
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
