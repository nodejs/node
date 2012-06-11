
module.exports = remotePackages

var npm = require("../../npm.js")
  , registry = npm.registry
  , containsSingleMatch = require("./contains-single-match.js")
  , getCompletions = require("./get-completions.js")

/*
  Looks up remote packages for CLI tab-completion.

  NOTE: If doVersion is true, versions in the form <name>@<version>
        will be completed.

        If doTag is true, tags in the form <name>@<tag> will be
        completed.

        If recurring in true, sequences of multiple packages can be
        completed. i.e. for schemes such as:
        npm <command> <name>[@<version> [<name>[@<version>] ...]
*/
function remotePackages (args, index, doVersion, doTag
                         , recurring, cb) {
  if (recurring || index < 3) {
    var name = (args.length + 1 === index) ? args[args.length - 1] : ""
    if (name === undefined) name = ""
    if (name.indexOf("/") !== -1) return cb(null, [])
    // use up-to 1 hour stale cache.  not super urgent.
    registry.get("/", 3600, function (er, d) {
      if (er) return cb(er)
      var remoteList = Object.keys(d)
        , found = remoteList.indexOf(name)
        , unique = found && containsSingleMatch(name, remoteList)
        , simpleMatches = getCompletions(name, remoteList)
        , uniqueMatch = unique && simpleMatches[0]
        , addTag = doTag && (unique || found || name.indexOf("@") !== -1)
        , addVer = doVersion && (unique || found || name.indexOf("@") !== -1)
        , list = []
        , pieces = (uniqueMatch || name).split("@")
        , pkgname = pieces[0]
        , extras = []
      if (unique && !addTag && !addVer) return cb(null, [uniqueMatch])
      if (d[pkgname] && (addTag || addVer)) {
        if (d[pkgname].versions && addVer) {
          extras = extras.concat(Object.keys(d[pkgname].versions))
        }
        if (d[pkgname]["dist-tags"] && addTag) {
          extras = extras.concat(Object.keys(d[pkgname]["dist-tags"]))
        }
        list = getCompletions(name, list.concat(extras.map(function (e) {
          return pkgname + "@" + e
        })))
      }
      if (!unique) list = list.concat(getCompletions(name, remoteList))
      return cb(null, list)
    })
  }
}
