// traverse the node_modules/package.json tree
// looking for duplicates.  If any duplicates are found,
// then move them up to the highest level necessary
// in order to make them no longer duplicated.
//
// This is kind of ugly, and really highlights the need for
// much better "put pkg X at folder Y" abstraction.  Oh well,
// whatever.  Perfect enemy of the good, and all that.

var fs = require("fs")
var asyncMap = require("slide").asyncMap
var path = require("path")
var readJson = require("read-package-json")
var archy = require("archy")
var util = require("util")
var RegClient = require("npm-registry-client")
var npmconf = require("npmconf")
var npm = require("npm")
var semver = require("semver")
var npm = require("npm")
var rimraf = require("rimraf")
var log = require("npmlog")
var npm = require("./npm.js")

module.exports = dedupe

dedupe.usage = "npm dedupe [pkg pkg...]"

function dedupe (args, silent, cb) {
  if (typeof silent === "function") cb = silent, silent = false
  var dryrun = false
  if (npm.command.match(/^find/)) dryrun = true
  return dedupe_(npm.prefix, args, {}, dryrun, silent, cb)
}

function dedupe_ (dir, filter, unavoidable, dryrun, silent, cb) {
  readInstalled(path.resolve(dir), {}, null, function (er, data, counter) {
    if (er) {
      return cb(er)
    }

    if (!data) {
      return cb()
    }

    // find out which things are dupes
    var dupes = Object.keys(counter || {}).filter(function (k) {
      if (filter.length && -1 === filter.indexOf(k)) return false
      return counter[k] > 1 && !unavoidable[k]
    }).reduce(function (s, k) {
      s[k] = []
      return s
    }, {})

    // any that are unavoidable need to remain as they are.  don't even
    // try to touch them or figure it out.  Maybe some day, we can do
    // something a bit more clever here, but for now, just skip over it,
    // and all its children.
    ;(function U (obj) {
      if (unavoidable[obj.name]) {
        obj.unavoidable = true
      }
      if (obj.parent && obj.parent.unavoidable) {
        obj.unavoidable = true
      }
      Object.keys(obj.children).forEach(function (k) {
        U(obj.children[k])
      })
    })

    // then collect them up and figure out who needs them
    ;(function C (obj) {
      if (dupes[obj.name] && !obj.unavoidable) {
        dupes[obj.name].push(obj)
        obj.duplicate = true
      }
      obj.dependents = whoDepends(obj)
      Object.keys(obj.children).forEach(function (k) {
        C(obj.children[k])
      })
    })(data)

    if (dryrun) {
      var k = Object.keys(dupes)
      if (!k.length) return cb()
      return npm.commands.ls(k, silent, cb)
    }

    var summary = Object.keys(dupes).map(function (n) {
      return [n, dupes[n].filter(function (d) {
        return d && d.parent && !d.parent.duplicate && !d.unavoidable
      }).map(function M (d) {
        return [d.path, d.version, d.dependents.map(function (k) {
          return [k.path, k.version, k.dependencies[d.name] || ""]
        })]
      })]
    }).map(function (item) {
      var name = item[0]
      var set = item[1]

      var ranges = set.map(function (i) {
        return i[2].map(function (d) {
          return d[2]
        })
      }).reduce(function (l, r) {
        return l.concat(r)
      }, []).map(function (v, i, set) {
        if (set.indexOf(v) !== i) return false
        return v
      }).filter(function (v) {
        return v !== false
      })

      var locs = set.map(function (i) {
        return i[0]
      })

      var versions = set.map(function (i) {
        return i[1]
      }).filter(function (v, i, set) {
        return set.indexOf(v) === i
      })

      var has = set.map(function (i) {
        return [i[0], i[1]]
      }).reduce(function (set, kv) {
        set[kv[0]] = kv[1]
        return set
      }, {})

      var loc = locs.length ? locs.reduce(function (a, b) {
        // a=/path/to/node_modules/foo/node_modules/bar
        // b=/path/to/node_modules/elk/node_modules/bar
        // ==/path/to/node_modules/bar
        a = a.split(/\/node_modules\//)
        b = b.split(/\/node_modules\//)
        var name = a.pop()
        b.pop()
        // find the longest chain that both A and B share.
        // then push the name back on it, and join by /node_modules/
        var res = []
        for (var i = 0, al = a.length, bl = b.length; i < al && i < bl && a[i] === b[i]; i++);
        return a.slice(0, i).concat(name).join("/node_modules/")
      }) : undefined

      return [item[0], { item: item
                       , ranges: ranges
                       , locs: locs
                       , loc: loc
                       , has: has
                       , versions: versions
                       }]
    }).filter(function (i) {
      return i[1].loc
    })

    findVersions(npm, summary, function (er, set) {
      if (er) return cb(er)
      if (!set.length) return cb()
      installAndRetest(set, filter, dir, unavoidable, silent, cb)
    })
  })
}

function installAndRetest (set, filter, dir, unavoidable, silent, cb) {
  //return cb(null, set)
  var remove = []

  asyncMap(set, function (item, cb) {
    // [name, has, loc, locMatch, regMatch, others]
    var name = item[0]
    var has = item[1]
    var where = item[2]
    var locMatch = item[3]
    var regMatch = item[4]
    var others = item[5]

    // nothing to be done here.  oh well.  just a conflict.
    if (!locMatch && !regMatch) {
      log.warn("unavoidable conflict", item[0], item[1])
      log.warn("unavoidable conflict", "Not de-duplicating")
      unavoidable[item[0]] = true
      return cb()
    }

    // nothing to do except to clean up the extraneous deps
    if (locMatch && has[where] === locMatch) {
      remove.push.apply(remove, others)
      return cb()
    }

    if (regMatch) {
      var what = name + "@" + regMatch
      // where is /path/to/node_modules/foo/node_modules/bar
      // for package "bar", but we need it to be just
      // /path/to/node_modules/foo
      where = where.split(/\/node_modules\//)
      where.pop()
      where = where.join("/node_modules/")
      remove.push.apply(remove, others)

      return npm.commands.install(where, what, cb)
    }

    // hrm?
    return cb(new Error("danger zone\n" + name + " " +
                        + regMatch + " " + locMatch))

  }, function (er, installed) {
    if (er) return cb(er)
    asyncMap(remove, rimraf, function (er) {
      if (er) return cb(er)
      remove.forEach(function (r) {
        log.info("rm", r)
      })
      dedupe_(dir, filter, unavoidable, false, silent, cb)
    })
  })
}

function findVersions (npm, summary, cb) {
  // now, for each item in the summary, try to find the maximum version
  // that will satisfy all the ranges.  next step is to install it at
  // the specified location.
  asyncMap(summary, function (item, cb) {
    var name = item[0]
    var data = item[1]
    var loc = data.loc
    var locs = data.locs.filter(function (l) {
      return l !== loc
    })

    // not actually a dupe, or perhaps all the other copies were
    // children of a dupe, so this'll maybe be picked up later.
    if (locs.length === 0) {
      return cb(null, [])
    }

    // { <folder>: <version> }
    var has = data.has

    // the versions that we already have.
    // if one of these is ok, then prefer to use that.
    // otherwise, try fetching from the registry.
    var versions = data.versions

    var ranges = data.ranges
    npm.registry.get(name, function (er, data) {
      var regVersions = er ? [] : Object.keys(data.versions)
      var locMatch = bestMatch(versions, ranges)
      var regMatch = bestMatch(regVersions, ranges)

      cb(null, [[name, has, loc, locMatch, regMatch, locs]])
    })
  }, cb)
}

function bestMatch (versions, ranges) {
  return versions.filter(function (v) {
    return !ranges.some(function (r) {
      return !semver.satisfies(v, r, true)
    })
  }).sort(semver.compareLoose).pop()
}


function readInstalled (dir, counter, parent, cb) {
  var pkg, children, realpath

  fs.realpath(dir, function (er, rp) {
    realpath = rp
    next()
  })

  readJson(path.resolve(dir, "package.json"), function (er, data) {
    if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
    if (er) return cb() // not a package, probably.
    counter[data.name] = counter[data.name] || 0
    counter[data.name]++
    pkg =
      { _id: data._id
      , name: data.name
      , version: data.version
      , dependencies: data.dependencies || {}
      , optionalDependencies: data.optionalDependencies || {}
      , devDependencies: data.devDependencies || {}
      , bundledDependencies: data.bundledDependencies || []
      , path: dir
      , realPath: dir
      , children: {}
      , parent: parent
      , family: Object.create(parent ? parent.family : null)
      , unavoidable: false
      , duplicate: false
      }
    if (parent) {
      parent.children[data.name] = pkg
      parent.family[data.name] = pkg
    }
    next()
  })

  fs.readdir(path.resolve(dir, "node_modules"), function (er, c) {
    children = c || [] // error is ok, just means no children.
    children = children.filter(function (p) {
      return !p.match(/^[\._-]/)
    })
    next()
  })

  function next () {
    if (!children || !pkg || !realpath) return

    // ignore devDependencies.  Just leave them where they are.
    children = children.filter(function (c) {
      return !pkg.devDependencies.hasOwnProperty(c)
    })

    pkg.realPath = realpath
    if (pkg.realPath !== pkg.path) children = []
    var d = path.resolve(dir, "node_modules")
    asyncMap(children, function (child, cb) {
      readInstalled(path.resolve(d, child), counter, pkg, cb)
    }, function (er) {
      cb(er, pkg, counter)
    })
  }
}

function whoDepends (pkg) {
  var start = pkg.parent || pkg
  return whoDepends_(pkg, [], start)
}

function whoDepends_ (pkg, who, test) {
  if (test !== pkg &&
      test.dependencies[pkg.name] &&
      test.family[pkg.name] === pkg) {
    who.push(test)
  }
  Object.keys(test.children).forEach(function (n) {
    whoDepends_(pkg, who, test.children[n])
  })
  return who
}

