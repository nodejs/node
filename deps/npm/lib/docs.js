
module.exports = docs

docs.usage = "npm docs <pkgname>"

docs.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  registry.get("/-/short", null, 60000, function (er, list) {
    return cb(null, list || [])
  })
}

var exec = require("./utils/exec.js")
  , registry = require("./utils/npm-registry-client/index.js")
  , npm = require("./npm.js")
  , log = require("./utils/log.js")

function docs (args, cb) {
  if (!args.length) return cb(docs.usage)
  var n = args[0].split("@").shift()
  registry.get(n, "latest", 3600, function (er, d) {
    if (er) return cb(er)
    var homepage = d.homepage
      , repo = d.repository || d.repositories
    if (homepage) return open(homepage, cb)
    if (repo) {
      if (Array.isArray(repo)) repo = repo.shift()
      if (repo.url) repo = repo.url
      log.verbose(repo, "repository")
      if (repo) {
        return open(repo.replace(/^git(@|:\/\/)/, 'http://')
                        .replace(/\.git$/, '')+"#readme", cb)
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
