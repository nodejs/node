
module.exports = repo

repo.usage = "npm repo <pkgname>"

var npm = require("./npm.js")
  , opener = require("opener")
  , github = require("github-url-from-git")
  , githubUserRepo = require("github-url-from-username-repo")
  , path = require("path")
  , readJson = require("read-package-json")
  , fs = require("fs")
  , url_ = require("url")
  , mapToRegistry = require("./utils/map-to-registry.js")
  , npa = require("npm-package-arg")

repo.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  mapToRegistry("/-/short", npm.config, function (er, uri) {
    if (er) return cb(er)

    npm.registry.get(uri, { timeout : 60000 }, function (er, list) {
      return cb(null, list || [])
    })
  })
}

function repo (args, cb) {
  var n = args.length && npa(args[0]).name || "."
  fs.stat(n, function (er, s) {
    if (er && er.code === "ENOENT") return callRegistry(n, cb)
    else if (er) return cb(er)
    if (!s.isDirectory()) return callRegistry(n, cb)
    readJson(path.resolve(n, "package.json"), function (er, d) {
      if (er) return cb(er)
      getUrlAndOpen(d, cb)
    })
  })
}

function getUrlAndOpen (d, cb) {
  var r = d.repository
  if (!r) return cb(new Error("no repository"))
  // XXX remove this when npm@v1.3.10 from node 0.10 is deprecated
  // from https://github.com/npm/npm-www/issues/418
  if (githubUserRepo(r.url))
    r.url = githubUserRepo(r.url)

  var url = (r.url && ~r.url.indexOf("github"))
          ? github(r.url)
          : nonGithubUrl(r.url)

  if (!url)
    return cb(new Error("no repository: could not get url"))
  opener(url, { command: npm.config.get("browser") }, cb)
}

function callRegistry (n, cb) {
  mapToRegistry(n, npm.config, function (er, uri) {
    if (er) return cb(er)

    npm.registry.get(uri + "/latest", { timeout : 3600 }, function (er, d) {
      if (er) return cb(er)
      getUrlAndOpen(d, cb)
    })
  })
}

function nonGithubUrl (url) {
  try {
    var idx = url.indexOf("@")
    if (idx !== -1) {
      url = url.slice(idx+1).replace(/:([^\d]+)/, "/$1")
    }
    url = url_.parse(url)
    var protocol = url.protocol === "https:"
                 ? "https:"
                 : "http:"
    return protocol + "//" + (url.host || "") +
      url.path.replace(/\.git$/, "")
  }
  catch(e) {}
}
