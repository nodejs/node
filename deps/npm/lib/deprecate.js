
module.exports = deprecate

deprecate.usage = "npm deprecate <pkg>[@<version>] <message>"

deprecate.completion = function (opts, cb) {
  // first, get a list of remote packages this user owns.
  // once we have a user account, then don't complete anything.
  var un = npm.config.get("username")
  if (!npm.config.get("username")) return cb()
  if (opts.conf.argv.remain.length > 2) return cb()
  // get the list of packages by user
  var uri = "/-/by-user/"+encodeURIComponent(un)
  registry.get(uri, null, 60000, function (er, list) {
    if (er) return cb()
    console.error(list)
    return cb(null, list[un])
  })
}

var registry = require("./utils/npm-registry-client/index.js")
  , semver = require("semver")
  , log = require("./utils/log.js")
  , npm = require("./npm.js")

function deprecate (args, cb) {
  var pkg = args[0]
    , msg = args[1]
  if (msg === undefined) return cb(new Error(deprecate.usage))
  // fetch the data and make sure it exists.
  pkg = pkg.split(/@/)
  var name = pkg.shift()
    , ver = pkg.join("@")
  if (semver.validRange(ver) === null) {
    return cb(new Error("invalid version range: "+ver))
  }
  registry.get(name, function (er, data) {
    if (er) return cb(er)
    // filter all the versions that match
    Object.keys(data.versions).filter(function (v) {
      return semver.satisfies(v, ver)
    }).forEach(function (v) {
      data.versions[v].deprecated = msg
    })
    // now update the doc on the registry
    registry.request.PUT(data._id, data, cb)
  })
}
