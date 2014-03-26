
// show the installed versions of packages
//
// --parseable creates output like this:
// <fullpath>:<name@ver>:<realpath>:<flags>
// Flags are a :-separated list of zero or more indicators

module.exports = exports = ls

var npm = require("./npm.js")
  , readInstalled = require("read-installed")
  , log = require("npmlog")
  , path = require("path")
  , archy = require("archy")
  , semver = require("semver")
  , url = require("url")
  , isGitUrl = require("./utils/is-git-url.js")

ls.usage = "npm ls"

ls.completion = require("./utils/completion/installed-deep.js")

function ls (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false

  var dir = path.resolve(npm.dir, "..")

  // npm ls 'foo@~1.3' bar 'baz@<2'
  if (!args) args = []
  else args = args.map(function (a) {
    var nv = a.split("@")
      , name = nv.shift()
      , ver = semver.validRange(nv.join("@")) || ""

    return [ name, ver ]
  })

  var depth = npm.config.get("depth")
  var opt = { depth: depth, log: log.warn, dev: true }
  readInstalled(dir, opt, function (er, data) {
    var bfs = bfsify(data, args)
      , lite = getLite(bfs)

    if (er || silent) return cb(er, data, lite)

    var long = npm.config.get("long")
      , json = npm.config.get("json")
      , out
    if (json) {
      var seen = []
      var d = long ? bfs : lite
      // the raw data can be circular
      out = JSON.stringify(d, function (k, o) {
        if (typeof o === "object") {
          if (-1 !== seen.indexOf(o)) return "[Circular]"
          seen.push(o)
        }
        return o
      }, 2)
    } else if (npm.config.get("parseable")) {
      out = makeParseable(bfs, long, dir)
    } else if (data) {
      out = makeArchy(bfs, long, dir)
    }
    console.log(out)

    if (args.length && !data._found) process.exitCode = 1

    // if any errors were found, then complain and exit status 1
    if (lite.problems && lite.problems.length) {
      er = lite.problems.join('\n')
    }
    cb(er, data, lite)
  })
}

// only include
function filter (data, args) {

}

function alphasort (a, b) {
  a = a.toLowerCase()
  b = b.toLowerCase()
  return a > b ? 1
       : a < b ? -1 : 0
}

function getLite (data, noname) {
  var lite = {}
    , maxDepth = npm.config.get("depth")

  if (!noname && data.name) lite.name = data.name
  if (data.version) lite.version = data.version
  if (data.extraneous) {
    lite.extraneous = true
    lite.problems = lite.problems || []
    lite.problems.push( "extraneous: "
                      + data.name + "@" + data.version
                      + " " + (data.path || "") )
  }

  if (data._from)
    lite.from = data._from

  if (data._resolved)
    lite.resolved = data._resolved

  if (data.invalid) {
    lite.invalid = true
    lite.problems = lite.problems || []
    lite.problems.push( "invalid: "
                      + data.name + "@" + data.version
                      + " " + (data.path || "") )
  }

  if (data.peerInvalid) {
    lite.peerInvalid = true
    lite.problems = lite.problems || []
    lite.problems.push( "peer invalid: "
                      + data.name + "@" + data.version
                      + " " + (data.path || "") )
  }

  if (data.dependencies) {
    var deps = Object.keys(data.dependencies)
    if (deps.length) lite.dependencies = deps.map(function (d) {
      var dep = data.dependencies[d]
      if (typeof dep === "string") {
        lite.problems = lite.problems || []
        var p
        if (data.depth >= maxDepth) {
          p = "max depth reached: "
        } else {
          p = "missing: "
        }
        p += d + "@" + dep
          + ", required by "
          + data.name + "@" + data.version
        lite.problems.push(p)
        return [d, { required: dep, missing: true }]
      }
      return [d, getLite(dep, true)]
    }).reduce(function (deps, d) {
      if (d[1].problems) {
        lite.problems = lite.problems || []
        lite.problems.push.apply(lite.problems, d[1].problems)
      }
      deps[d[0]] = d[1]
      return deps
    }, {})
  }
  return lite
}

function bfsify (root, args, current, queue, seen) {
  // walk over the data, and turn it from this:
  // +-- a
  // |   `-- b
  // |       `-- a (truncated)
  // `--b (truncated)
  // into this:
  // +-- a
  // `-- b
  // which looks nicer
  args = args || []
  current = current || root
  queue = queue || []
  seen = seen || [root]
  var deps = current.dependencies = current.dependencies || {}
  Object.keys(deps).forEach(function (d) {
    var dep = deps[d]
    if (typeof dep !== "object") return
    if (seen.indexOf(dep) !== -1) {
      if (npm.config.get("parseable") || !npm.config.get("long")) {
        delete deps[d]
        return
      } else {
        dep = deps[d] = Object.create(dep)
        dep.dependencies = {}
      }
    }
    queue.push(dep)
    seen.push(dep)
  })

  if (!queue.length) {
    // if there were args, then only show the paths to found nodes.
    return filterFound(root, args)
  }
  return bfsify(root, args, queue.shift(), queue, seen)
}

function filterFound (root, args) {
  if (!args.length) return root
  var deps = root.dependencies
  if (deps) Object.keys(deps).forEach(function (d) {
    var dep = filterFound(deps[d], args)

    // see if this one itself matches
    var found = false
    for (var i = 0; !found && i < args.length; i ++) {
      if (d === args[i][0]) {
        found = semver.satisfies(dep.version, args[i][1], true)
      }
    }
    // included explicitly
    if (found) dep._found = true
    // included because a child was included
    if (dep._found && !root._found) root._found = 1
    // not included
    if (!dep._found) delete deps[d]
  })
  if (!root._found) root._found = false
  return root
}

function makeArchy (data, long, dir) {
  var out = makeArchy_(data, long, dir, 0)
  return archy(out, "", { unicode: npm.config.get("unicode") })
}

function makeArchy_ (data, long, dir, depth, parent, d) {
  var color = npm.color
  if (typeof data === "string") {
    if (depth < npm.config.get("depth")) {
      // just missing
      var p = parent.link || parent.path
      var unmet = "UNMET DEPENDENCY"
      if (color) {
        unmet = "\033[31;40m" + unmet + "\033[0m"
      }
      data = unmet + " " + d + " " + data
    } else {
      data = d+"@"+ data
    }
    return data
  }

  var out = {}
  // the top level is a bit special.
  out.label = data._id || ""
  if (data._found === true && data._id) {
    var pre = color ? "\033[33;40m" : ""
      , post = color ? "\033[m" : ""
    out.label = pre + out.label.trim() + post + " "
  }
  if (data.link) out.label += " -> " + data.link

  if (data.invalid) {
    if (data.realName !== data.name) out.label += " ("+data.realName+")"
    out.label += " " + (color ? "\033[31;40m" : "")
              + "invalid"
              + (color ? "\033[0m" : "")
  }

  if (data.peerInvalid) {
    out.label += " " + (color ? "\033[31;40m" : "")
              + "peer invalid"
              + (color ? "\033[0m" : "")
  }

  if (data.extraneous && data.path !== dir) {
    out.label += " " + (color ? "\033[32;40m" : "")
              + "extraneous"
              + (color ? "\033[0m" : "")
  }

  // add giturl to name@version
  if (data._resolved) {
    var p = url.parse(data._resolved)
    if (isGitUrl(p))
      out.label += " (" + data._resolved + ")"
  }

  if (long) {
    if (dir === data.path) out.label += "\n" + dir
    out.label += "\n" + getExtras(data, dir)
  } else if (dir === data.path) {
    if (out.label) out.label += " "
    out.label += dir
  }

  // now all the children.
  out.nodes = Object.keys(data.dependencies || {})
    .sort(alphasort).map(function (d) {
      return makeArchy_(data.dependencies[d], long, dir, depth + 1, data, d)
    })

  if (out.nodes.length === 0 && data.path === dir) {
    out.nodes = ["(empty)"]
  }

  return out
}

function getExtras (data, dir) {
  var extras = []

  if (data.description) extras.push(data.description)
  if (data.repository) extras.push(data.repository.url)
  if (data.homepage) extras.push(data.homepage)
  if (data._from) {
    var from = data._from
    if (from.indexOf(data.name + "@") === 0) {
      from = from.substr(data.name.length + 1)
    }
    var u = url.parse(from)
    if (u.protocol) extras.push(from)
  }
  return extras.join("\n")
}


function makeParseable (data, long, dir, depth, parent, d) {
  depth = depth || 0

  return [ makeParseable_(data, long, dir, depth, parent, d) ]
  .concat(Object.keys(data.dependencies || {})
    .sort(alphasort).map(function (d) {
      return makeParseable(data.dependencies[d], long, dir, depth + 1, data, d)
    }))
  .filter(function (x) { return x })
  .join("\n")
}

function makeParseable_ (data, long, dir, depth, parent, d) {
  if (data.hasOwnProperty("_found") && data._found !== true) return ""

  if (typeof data === "string") {
    if (data.depth < npm.config.get("depth")) {
      var p = parent.link || parent.path
      data = npm.config.get("long")
           ? path.resolve(parent.path, "node_modules", d)
           + ":"+d+"@"+JSON.stringify(data)+":INVALID:MISSING"
           : ""
    } else {
      data = path.resolve(data.path || "", "node_modules", d || "")
           + (npm.config.get("long")
             ? ":" + d + "@" + JSON.stringify(data)
             + ":" // no realpath resolved
             + ":MAXDEPTH"
             : "")
    }

    return data
  }

  if (!npm.config.get("long")) return data.path

  return data.path
       + ":" + (data._id || "")
       + ":" + (data.realPath !== data.path ? data.realPath : "")
       + (data.extraneous ? ":EXTRANEOUS" : "")
       + (data.invalid ? ":INVALID" : "")
       + (data.peerInvalid ? ":PEERINVALID" : "")
}
