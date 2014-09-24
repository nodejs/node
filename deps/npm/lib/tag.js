// turns out tagging isn't very complicated
// all the smarts are in the couch.
module.exports = tag
tag.usage = "npm tag <project>@<version> [<tag>]"

tag.completion = require("./unpublish.js").completion

var npm = require("./npm.js")
  , registry = npm.registry
  , mapToRegistry = require("./utils/map-to-registry.js")
  , npa = require("npm-package-arg")
  , semver = require("semver")

function tag (args, cb) {
  var thing = npa(args.shift() || "")
    , project = thing.name
    , version = thing.rawSpec
    , t = args.shift() || npm.config.get("tag")

  t = t.trim()

  if (!project || !version || !t) return cb("Usage:\n"+tag.usage)

  if (semver.validRange(t)) {
    var er = new Error("Tag name must not be a valid SemVer range: " + t)
    return cb(er)
  }

  mapToRegistry(project, npm.config, function (er, uri) {
    if (er) return cb(er)

    registry.tag(uri, version, t, cb)
  })
}
