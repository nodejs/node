
module.exports = repo

repo.usage = "npm repo <pkgname>"

repo.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  registry.get("/-/short", 60000, function (er, list) {
    return cb(null, list || [])
  })
}

var npm = require("./npm.js")
  , registry = npm.registry
  , log = require("npmlog")
  , opener = require("opener")
  , github = require('github-url-from-git')
  , githubUserRepo = require("github-url-from-username-repo")
  , path = require("path")
  , readJson = require("read-package-json")
  , fs = require("fs")

function repo (args, cb) {
  var n = args.length && args[0].split("@").shift() || '.'
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
  var r = d.repository;
  if (!r) return cb(new Error('no repository'));
  // XXX remove this when npm@v1.3.10 from node 0.10 is deprecated
  // from https://github.com/isaacs/npm-www/issues/418
  if (githubUserRepo(r.url))
    r.url = githubUserRepo(r.url)
  var url = github(r.url)
  if (!url)
    return cb(new Error('no repository: could not get url'))
  opener(url, { command: npm.config.get("browser") }, cb)
}

function callRegistry (n, cb) {
  registry.get(n + "/latest", 3600, function (er, d) {
    if (er) return cb(er)
    getUrlAndOpen(d, cb)
  })
}
