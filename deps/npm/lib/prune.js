// prune extraneous packages.

module.exports = prune

prune.usage = "npm prune"

var readInstalled = require("read-installed")
  , npm = require("./npm.js")

prune.completion = require("./utils/completion/installed-deep.js")

function prune (args, cb) {
  readInstalled(npm.prefix, npm.config.get("depth"), function (er, data) {
    if (er) return cb(er)
    prune_(args, data, cb)
  })
}

function prune_ (args, data, cb) {
  npm.commands.unbuild(prunables(args, data, []), cb)
}

function prunables (args, data, seen) {
  var deps = data.dependencies || {}
  return Object.keys(deps).map(function (d) {
    if (typeof deps[d] !== "object"
        || seen.indexOf(deps[d]) !== -1) return null
    seen.push(deps[d])
    if (deps[d].extraneous
        && (args.length === 0 || args.indexOf(d) !== -1)) {
      var extra = deps[d]
      delete deps[d]
      return extra.path
    }
    return prunables(args, deps[d], seen)
  }).filter(function (d) { return d !== null })
  .reduce(function FLAT (l, r) {
    return l.concat(Array.isArray(r) ? r.reduce(FLAT,[]) : r)
  }, [])
}
