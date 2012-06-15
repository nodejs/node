
// show the installed versions of packages
//
// --parseable creates output like this:
// <fullpath>:<name@ver>:<realpath>:<flags>
// Flags are a :-separated list of zero or more indicators

module.exports = exports = ls

var npm = require("./npm.js")
  , readInstalled = require("read-installed")
  , output = require("./utils/output.js")
  , log = require("npmlog")
  , path = require("path")
  , archy = require("archy")

ls.usage = "npm ls"

function ls (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false

  if (args.length) {
    // TODO: it would actually be nice to maybe show the locally
    // installed packages only matching the argument names.
    log.warn("ls doesn't take positional args. Try the 'search' command")
  }

  var dir = path.resolve(npm.dir, "..")

  readInstalled(dir, npm.config.get("depth"), function (er, data) {
    var lite = getLite(bfsify(data))
    if (er || silent) return cb(er, data, lite)

    var long = npm.config.get("long")
      , json = npm.config.get("json")
      , out
    if (json) {
      var seen = []
      var d = long ? bfsify(data) : lite
      // the raw data can be circular
      out = JSON.stringify(d, function (k, o) {
        if (typeof o === "object") {
          if (-1 !== seen.indexOf(o)) return "[Circular]"
          seen.push(o)
        }
        return o
      }, 2)
    } else if (npm.config.get("parseable")) {
      out = makeParseable(bfsify(data), long, dir)
    } else if (data) {
      out = makeArchy(bfsify(data), long, dir)
    }
    output.write(out, function (er) { cb(er, data, lite) })
  })
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
    , url = require("url")

  if (!noname && data.name) lite.name = data.name
  if (data.version) lite.version = data.version
  if (data.extraneous) {
    lite.extraneous = true
    lite.problems = lite.problems || []
    lite.problems.push( "extraneous: "
                      + data.name + "@" + data.version
                      + " " + (data.path || "") )
  }

  if (data._from) {
    var from = data._from
    if (from.indexOf(data.name + "@") === 0) {
      from = from.substr(data.name.length + 1)
    }
    var u = url.parse(from)
    if (u.protocol) lite.from = from
  }

  if (data.invalid) {
    lite.invalid = true
    lite.problems = lite.problems || []
    lite.problems.push( "invalid: "
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

function bfsify (root, current, queue, seen) {
  // walk over the data, and turn it from this:
  // +-- a
  // |   `-- b
  // |       `-- a (truncated)
  // `--b (truncated)
  // into this:
  // +-- a
  // `-- b
  // which looks nicer
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
  if (!queue.length) return root
  return bfsify(root, queue.shift(), queue, seen)
}


function makeArchy (data, long, dir) {
  var out = makeArchy_(data, long, dir, 0)
  return archy(out, "", { unicode: npm.config.get("unicode") })
}

function makeArchy_ (data, long, dir, depth, parent, d) {
  if (typeof data === "string") {
    if (depth < npm.config.get("depth")) {
      // just missing
      var p = parent.link || parent.path
      log.warn("unmet dependency", "%s in %s", d+" "+data, p)
      data = "\033[31;40mUNMET DEPENDENCY\033[0m " + d + " " + data
    } else {
      data = d+"@"+ data +" (max depth reached)"
    }
    return data
  }

  var out = {}
  // the top level is a bit special.
  out.label = data._id ? data._id + " " : ""
  if (data.link) out.label += "-> " + data.link

  if (data.invalid) {
    if (data.realName !== data.name) out.label += " ("+data.realName+")"
    out.label += " \033[31;40minvalid\033[0m"
  }

  if (data.extraneous && data.path !== dir) {
    out.label += " \033[32;40mextraneous\033[0m"
  }

  if (long) {
    if (dir === data.path) out.label += "\n" + dir
    out.label += "\n" + getExtras(data, dir)
  } else if (dir === data.path) {
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
    , url = require("url")

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
  .join("\n")
}

function makeParseable_ (data, long, dir, depth, parent, d) {
  if (typeof data === "string") {
    if (data.depth < npm.config.get("depth")) {
      var p = parent.link || parent.path
      log.warn("unmet dependency", "%s in %s", d+" "+data, p)
      data = npm.config.get("long")
           ? path.resolve(parent.path, "node_modules", d)
           + ":"+d+"@"+JSON.stringify(data)+":INVALID:MISSING"
           : ""
    } else {
      data = path.resolve(data.path, "node_modules", d)
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
}
