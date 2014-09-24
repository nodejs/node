
module.exports = bugs

bugs.usage = "npm bugs <pkgname>"

var npm = require("./npm.js")
  , registry = npm.registry
  , log = require("npmlog")
  , opener = require("opener")
  , path = require("path")
  , readJson = require("read-package-json")
  , npa = require("npm-package-arg")
  , fs = require("fs")
  , mapToRegistry = require("./utils/map-to-registry.js")

bugs.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  mapToRegistry("-/short", npm.config, function (er, uri) {
    if (er) return cb(er)

    registry.get(uri, { timeout : 60000 }, function (er, list) {
      return cb(null, list || [])
    })
  })
}

function bugs (args, cb) {
  var n = args.length && npa(args[0]).name || '.'
  fs.stat(n, function (er, s) {
    if (er && er.code === "ENOENT") return callRegistry(n, cb)
    else if (er) return cb (er)
    if (!s.isDirectory()) return callRegistry(n, cb)
    readJson(path.resolve(n, "package.json"), function(er, d) {
      if (er) return cb(er)
      getUrlAndOpen(d, cb)
    })
  })
}

function getUrlAndOpen (d, cb) {
  var bugs = d.bugs
    , repo = d.repository || d.repositories
    , url
  if (bugs) {
    url = (typeof url === "string") ? bugs : bugs.url
  } else if (repo) {
    if (Array.isArray(repo)) repo = repo.shift()
    if (repo.hasOwnProperty("url")) repo = repo.url
    log.verbose("repository", repo)
    if (bugs && bugs.match(/^(https?:\/\/|git(:\/\/|@))github.com/)) {
      url = bugs.replace(/^git(@|:\/\/)/, "https://")
                .replace(/^https?:\/\/github.com:/, "https://github.com/")
                .replace(/\.git$/, '')+"/issues"
    }
  }
  if (!url) {
    url = "https://npmjs.org/package/" + d.name
  }
  opener(url, { command: npm.config.get("browser") }, cb)
}

function callRegistry (n, cb) {
  mapToRegistry(n, npm.config, function (er, uri) {
    if (er) return cb(er)

    registry.get(uri + "/latest", { timeout : 3600 }, function (er, d) {
      if (er) return cb(er)

      getUrlAndOpen (d, cb)
    })
  })
}
