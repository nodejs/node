
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

function repo (args, cb) {
  if (!args.length) return cb(repo.usage)
  var n = args[0].split("@").shift()
  registry.get(n + "/latest", 3600, function (er, d) {
    if (er) return cb(er)
    var r = d.repository;
    if (!r) return cb(new Error('no repository'));
    var url = github(r.url);
    opener(url, { command: npm.config.get("browser") }, cb)
  })
}
