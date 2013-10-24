
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

function repo (args, cb) {
  if (!args.length) return cb(repo.usage)
  var n = args[0].split("@").shift()
  registry.get(n + "/latest", 3600, function (er, d) {
    if (er) return cb(er)
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
  })
}
