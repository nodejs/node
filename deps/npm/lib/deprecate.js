var url = require("url")
  , npm = require("./npm.js")

module.exports = deprecate

deprecate.usage = "npm deprecate <pkg>[@<version>] <message>"

deprecate.completion = function (opts, cb) {
  // first, get a list of remote packages this user owns.
  // once we have a user account, then don't complete anything.
  var un = npm.config.get("username")
  if (!npm.config.get("username")) return cb()
  if (opts.conf.argv.remain.length > 2) return cb()
  // get the list of packages by user
  var path = "/-/by-user/"+encodeURIComponent(un)
    , uri = url.resolve(npm.config.get("registry"), path)
  npm.registry.get(uri, { timeout : 60000 }, function (er, list) {
    if (er) return cb()
    console.error(list)
    return cb(null, list[un])
  })
}

function deprecate (args, cb) {
  var pkg = args[0]
    , msg = args[1]
  if (msg === undefined) return cb("Usage: " + deprecate.usage)
  // fetch the data and make sure it exists.
  pkg = pkg.split(/@/)
  var name = pkg.shift()
    , ver = pkg.join("@")
    , uri = url.resolve(npm.config.get("registry"), name)

  npm.registry.deprecate(uri, ver, msg, cb)
}
