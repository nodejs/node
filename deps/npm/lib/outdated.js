/*

npm outdated [pkg]

Does the following:

1. check for a new version of pkg

If no packages are specified, then run for all installed
packages.

--parseable creates output like this:
<fullpath>:<name@wanted>:<name@installed>:<name@latest>

*/

module.exports = outdated

outdated.usage = "npm outdated [<pkg> [<pkg> ...]]"

outdated.completion = require("./utils/completion/installed-deep.js")


var path = require("path")
  , readJson = require("read-package-json")
  , cache = require("./cache.js")
  , asyncMap = require("slide").asyncMap
  , npm = require("./npm.js")
  , url = require("url")
  , color = require("ansicolors")
  , styles = require("ansistyles")
  , table = require("text-table")
  , semver = require("semver")
  , os = require("os")
  , mapToRegistry = require("./utils/map-to-registry.js")
  , npa = require("npm-package-arg")
  , readInstalled = require("read-installed")
  , long = npm.config.get("long")
  , log = require("npmlog")

function outdated (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false
  var dir = path.resolve(npm.dir, "..")

  // default depth for `outdated` is 0 (cf. `ls`)
  if (npm.config.get("depth") === Infinity) npm.config.set("depth", 0)

  outdated_(args, dir, {}, 0, function (er, list) {
    if (!list) list = []
    if (er || silent || list.length === 0) return cb(er, list)
    if (npm.config.get("json")) {
      console.log(makeJSON(list))
    } else if (npm.config.get("parseable")) {
      console.log(makeParseable(list))
    } else {
      var outList = list.map(makePretty)
      var outHead = [ "Package"
                    , "Current"
                    , "Wanted"
                    , "Latest"
                    , "Location"
                    ]
      if (long) outHead.push("Package Type")
      var outTable = [outHead].concat(outList)

      if (npm.color) {
        outTable[0] = outTable[0].map(function(heading) {
          return styles.underline(heading)
        })
      }

      var tableOpts = { align: ["l", "r", "r", "r", "l"]
                      , stringLength: function(s) { return ansiTrim(s).length }
                      }
      console.log(table(outTable, tableOpts))
    }
    cb(null, list)
  })
}

// [[ dir, dep, has, want, latest, type ]]
function makePretty (p) {
  var dep = p[1]
    , dir = path.resolve(p[0], "node_modules", dep)
    , has = p[2]
    , want = p[3]
    , latest = p[4]
    , type = p[6]

  if (!npm.config.get("global")) {
    dir = path.relative(process.cwd(), dir)
  }

  var columns = [ dep
                , has || "MISSING"
                , want
                , latest
                , dirToPrettyLocation(dir)
                ]
  if (long) columns[5] = type

  if (npm.color) {
    columns[0] = color[has === want ? "yellow" : "red"](columns[0]) // dep
    columns[2] = color.green(columns[2]) // want
    columns[3] = color.magenta(columns[3]) // latest
    columns[4] = color.brightBlack(columns[4]) // dir
    if (long) columns[5] = color.brightBlack(columns[5]) // type
  }

  return columns
}

function ansiTrim (str) {
  var r = new RegExp("\x1b(?:\\[(?:\\d+[ABCDEFGJKSTm]|\\d+;\\d+[Hfm]|" +
        "\\d+;\\d+;\\d+m|6n|s|u|\\?25[lh])|\\w)", "g")
  return str.replace(r, "")
}

function dirToPrettyLocation (dir) {
  return dir.replace(/^node_modules[/\\]/, "")
            .replace(/[[/\\]node_modules[/\\]/g, " > ")
}

function makeParseable (list) {
  return list.map(function (p) {

    var dep = p[1]
      , dir = path.resolve(p[0], "node_modules", dep)
      , has = p[2]
      , want = p[3]
      , latest = p[4]
      , type = p[6]

    var out = [ dir
           , dep + "@" + want
           , (has ? (dep + "@" + has) : "MISSING")
           , dep + "@" + latest
           ]
   if (long) out.push(type)

   return out.join(":")
  }).join(os.EOL)
}

function makeJSON (list) {
  var out = {}
  list.forEach(function (p) {
    var dir = path.resolve(p[0], "node_modules", p[1])
    if (!npm.config.get("global")) {
      dir = path.relative(process.cwd(), dir)
    }
    out[p[1]] = { current: p[2]
                , wanted: p[3]
                , latest: p[4]
                , location: dir
                }
    if (long) out[p[1]].type = p[6]
  })
  return JSON.stringify(out, null, 2)
}

function outdated_ (args, dir, parentHas, depth, cb) {
  // get the deps from package.json, or {<dir/node_modules/*>:"*"}
  // asyncMap over deps:
  //   shouldHave = cache.add(dep, req).version
  //   if has === shouldHave then
  //     return outdated(args, dir/node_modules/dep, parentHas + has)
  //   else if dep in args or args is empty
  //     return [dir, dep, has, shouldHave]

  if (depth > npm.config.get("depth")) {
    return cb(null, [])
  }
  var deps = null
  var types = {}
  readJson(path.resolve(dir, "package.json"), function (er, d) {
    d = d || {}
    if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
    deps = (er) ? true : (d.dependencies || {})
    if (!er) {
      Object.keys(deps).forEach(function (k) {
        types[k] = "dependencies"
      })
    }

    if (npm.config.get("save-dev")) {
      deps = d.devDependencies || {}
      Object.keys(deps).forEach(function (k) {
        types[k] = "devDependencies"
      })

      return next()
    }

    if (npm.config.get("save")) {
      // remove optional dependencies from dependencies during --save.
      Object.keys(d.optionalDependencies || {}).forEach(function (k) {
        delete deps[k]
      })
      return next()
    }

    if (npm.config.get("save-optional")) {
      deps = d.optionalDependencies || {}
      Object.keys(deps).forEach(function (k) {
        types[k] = "optionalDependencies"
      })
      return next()
    }

    var doUpdate = npm.config.get("dev") ||
                    (!npm.config.get("production") &&
                    !Object.keys(parentHas).length &&
                    !npm.config.get("global"))

    if (!er && d && doUpdate) {
      Object.keys(d.devDependencies || {}).forEach(function (k) {
        if (!(k in parentHas)) {
          deps[k] = d.devDependencies[k]
          types[k] = "devDependencies"
        }
      })
    }
    return next()
  })

  var has = null
  readInstalled(path.resolve(dir), { dev : true }, function (er, data) {
    if (er) {
      has = Object.create(parentHas)
      return next()
    }
    var pkgs = Object.keys(data.dependencies)
    pkgs = pkgs.filter(function (p) {
      return !p.match(/^[\._-]/)
    })
    asyncMap(pkgs, function (pkg, cb) {
      var jsonFile = path.resolve(dir, "node_modules", pkg, "package.json")
      readJson(jsonFile, function (er, d) {
        if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
        if (d && d.name && d.private) delete deps[d.name]
        cb(null, er ? [] : [[d.name, d.version, d._from]])
      })
    }, function (er, pvs) {
      if (er) return cb(er)
      has = Object.create(parentHas)
      pvs.forEach(function (pv) {
        has[pv[0]] = {
          version: pv[1],
          from: pv[2]
        }
      })

      next()
    })
  })

  function next () {
    if (!has || !deps) return
    if (deps === true) {
      deps = Object.keys(has).reduce(function (l, r) {
        l[r] = "latest"
        return l
      }, {})
    }

    // now get what we should have, based on the dep.
    // if has[dep] !== shouldHave[dep], then cb with the data
    // otherwise dive into the folder
    asyncMap(Object.keys(deps), function (dep, cb) {
      if (!long) return shouldUpdate(args, dir, dep, has, deps[dep], depth, cb)

      shouldUpdate(args, dir, dep, has, deps[dep], depth, cb, types[dep])
    }, cb)
  }
}

function shouldUpdate (args, dir, dep, has, req, depth, cb, type) {
  // look up the most recent version.
  // if that's what we already have, or if it's not on the args list,
  // then dive into it.  Otherwise, cb() with the data.

  // { version: , from: }
  var curr = has[dep]

  function skip (er) {
    // show user that no viable version can be found
    if (er) return cb(er)
    outdated_( args
             , path.resolve(dir, "node_modules", dep)
             , has
             , depth + 1
             , cb )
  }

  function doIt (wanted, latest) {
    if (!long) {
      return cb(null, [[ dir, dep, curr && curr.version, wanted, latest, req]])
    }
    cb(null, [[ dir, dep, curr && curr.version, wanted, latest, req, type]])
  }

  if (args.length && args.indexOf(dep) === -1) return skip()
  var parsed = npa(dep + '@' + req)
  if (parsed.type === "git" || (parsed.hosted && parsed.hosted.type === "github")) {
    return doIt("git", "git")
  }

  // search for the latest package
  mapToRegistry(dep, npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    npm.registry.get(uri, { auth : auth }, updateDeps)
  })

  function updateLocalDeps (latestRegistryVersion) {
    readJson(path.resolve(parsed.spec, 'package.json'), function (er, localDependency) {
      if (er) return cb()

      var wanted = localDependency.version
      var latest = localDependency.version

      if (latestRegistryVersion) {
        latest = latestRegistryVersion
        if (semver.lt(wanted, latestRegistryVersion)) {
          wanted = latestRegistryVersion
          req = dep + '@' + latest
        }
      }

      if (curr.version !== wanted) {
        doIt(wanted, latest)
      } else {
        skip()
      }
    })
  }

  function updateDeps (er, d) {
    if (er) {
      if (parsed.type !== 'local') return cb()
      return updateLocalDeps()
    }

    if (!d || !d["dist-tags"] || !d.versions) return cb()
    var l = d.versions[d["dist-tags"].latest]
    if (!l) return cb()

    var r = req
    if (d["dist-tags"][req])
      r = d["dist-tags"][req]

    if (semver.validRange(r, true)) {
      // some kind of semver range.
      // see if it's in the doc.
      var vers = Object.keys(d.versions)
      var v = semver.maxSatisfying(vers, r, true)
      if (v) {
        return onCacheAdd(null, d.versions[v])
      }
    }

    // We didn't find the version in the doc.  See if cache can find it.
    cache.add(dep, req, null, false, onCacheAdd)

    function onCacheAdd(er, d) {
      // if this fails, then it means we can't update this thing.
      // it's probably a thing that isn't published.
      if (er) {
        if (er.code && er.code === "ETARGET") {
          // no viable version found
          return skip(er)
        }
        return skip()
      }

      // check that the url origin hasn't changed (#1727) and that
      // there is no newer version available
      var dFromUrl = d._from && url.parse(d._from).protocol
      var cFromUrl = curr && curr.from && url.parse(curr.from).protocol

      if (!curr || dFromUrl && cFromUrl && d._from !== curr.from
          || d.version !== curr.version
          || d.version !== l.version) {
        if (parsed.type === 'local') return updateLocalDeps(l.version)

        doIt(d.version, l.version)
      }
      else {
        skip()
      }
    }
  }
}
