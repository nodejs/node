
module.exports = bugs

bugs.usage = "npm bugs <pkgname>"

var npm = require("./npm.js")
  , log = require("npmlog")
  , opener = require("opener")
  , path = require("path")
  , readJson = require("read-package-json")
  , npa = require("npm-package-arg")
  , fs = require("fs")
  , mapToRegistry = require("./utils/map-to-registry.js")

bugs.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  mapToRegistry("-/short", npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    npm.registry.get(uri, { timeout : 60000, auth : auth }, function (er, list) {
      return cb(null, list || [])
    })
  })
}

function bugs (args, cb) {
  var n = args.length && npa(args[0]).name || "."
  fs.stat(n, function (er, s) {
    if (er) {
      if (er.code === "ENOENT") return callRegistry(n, cb)
      return cb(er)
    }
    if (!s.isDirectory()) return callRegistry(n, cb)
    readJson(path.resolve(n, "package.json"), function(er, d) {
      if (er) return cb(er)
      getUrlAndOpen(d, cb)
    })
  })
}

function getUrlAndOpen (d, cb) {
  var repo = d.repository || d.repositories
    , url
  if (d.bugs) {
    url = (typeof d.bugs === "string") ? d.bugs : d.bugs.url
  }
  else if (repo) {
    if (Array.isArray(repo)) repo = repo.shift()
    if (repo.hasOwnProperty("url")) repo = repo.url
    log.verbose("bugs", "repository", repo)
    if (repo && repo.match(/^(https?:\/\/|git(:\/\/|@))github.com/)) {
      url = repo.replace(/^git(@|:\/\/)/, "https://")
                .replace(/^https?:\/\/github.com:/, "https://github.com/")
                .replace(/\.git$/, "")+"/issues"
    }
  }
  if (!url) {
    url = "https://www.npmjs.org/package/" + d.name
  }
  log.silly("bugs", "url", url)
  opener(url, { command: npm.config.get("browser") }, cb)
}

function callRegistry (name, cb) {
  mapToRegistry(name, npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    npm.registry.get(uri + "/latest", { auth : auth }, function (er, d) {
      if (er) return cb(er)

      getUrlAndOpen(d, cb)
    })
  })
}
