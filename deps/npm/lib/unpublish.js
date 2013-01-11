
module.exports = unpublish

var log = require("npmlog")
  , npm = require("./npm.js")
  , registry = npm.registry
  , readJson = require("read-package-json")
  , path = require("path")

unpublish.usage = "npm unpublish <project>[@<version>]"

unpublish.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length >= 3) return cb()
  var un = encodeURIComponent(npm.config.get("username"))
  if (!un) return cb()
  registry.get("/-/by-user/"+un, function (er, pkgs) {
    // do a bit of filtering at this point, so that we don't need
    // to fetch versions for more than one thing, but also don't
    // accidentally a whole project.
    pkgs = pkgs[un]
    if (!pkgs || !pkgs.length) return cb()
    var partial = opts.partialWord.split("@")
      , pp = partial.shift()
      , pv = partial.join("@")
    pkgs = pkgs.filter(function (p) {
      return p.indexOf(pp) === 0
    })
    if (pkgs.length > 1) return cb(null, pkgs)
    registry.get(pkgs[0], function (er, d) {
      if (er) return cb(er)
      var vers = Object.keys(d.versions)
      if (!vers.length) return cb(null, pkgs)
      return cb(null, vers.map(function (v) {
        return pkgs[0]+"@"+v
      }))
    })
  })
}

function unpublish (args, cb) {

  if (args.length > 1) return cb(unpublish.usage)

  var thing = args.length ? args.shift().split("@") : []
    , project = thing.shift()
    , version = thing.join("@")

  if (!version && !npm.config.get("force")) {
    return cb("Refusing to delete entire project.\n"
             +"Run with --force to do this.\n"
             +unpublish.usage)
  }

  if (!project || path.resolve(project) === npm.prefix) {
    // if there's a package.json in the current folder, then
    // read the package name and version out of that.
    var cwdJson = path.join(process.cwd(), "package.json")
    return readJson(cwdJson, function (er, data) {
      if (er && er.code !== "ENOENT") return cb(er)
      if (er) return cb("Usage:\n"+unpublish.usage)
      gotProject(data.name, data.version, cb)
    })
  }
  return gotProject(project, version, cb)
}

function gotProject (project, version, cb_) {
  function cb (er) {
    if (er) return cb_(er)
    console.log("- " + project + (version ? "@" + version : ""))
    cb_()
  }

  // remove from the cache first
  npm.commands.cache(["clean", project, version], function (er) {
    if (er) {
      log.error("unpublish", "Failed to clean cache")
      return cb(er)
    }

    registry.unpublish(project, version, cb)
  })
}
