
// show the installed versions of packages
//
// --parseable creates output like this:
// <fullpath>:<name@ver>:<realpath>:<flags>
// Flags are a :-separated list of zero or more indicators

module.exports = exports = ls

var npm = require("./npm.js")
  , readInstalled = require("./utils/read-installed.js")
  , output = require("./utils/output.js")
  , log = require("./utils/log.js")
  , relativize = require("./utils/relativize.js")
  , path = require("path")

ls.usage = "npm ls"

function ls (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false

  if (args.length) {
    log.warn("ls doesn't take positional args. Try the 'search' command")
  }

  var dir = path.resolve(npm.dir, "..")

  readInstalled(dir, function (er, data) {
    if (er || silent) return cb(er, data)
    var long = npm.config.get("long")
    var out = makePretty(bfsify(data), long, dir).join("\n")
    output.write(out, function (er) { cb(er, data) })
  })
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


function makePretty (data, long, dir, prefix, list) {
  var top = !list
  list = list || []
  prefix = prefix || ""
  list.push(format(data, long, prefix, dir))
  var deps = data.dependencies || {}
    , childPref = prefix.split("├─").join("│ ")
                        .split("└─").join("  ")
    , depList = Object.keys(deps)
    , depLast = depList.length - 1
    , maxDepth = npm.config.get("depth")
  Object.keys(deps).sort(function (a, b) {
    return a > b ? 1 : -1
  }).forEach(function (d, i) {
    var depData = deps[d]
    if (typeof depData === "string") {
      if (data.depth < maxDepth) {
        var p = data.link || data.path
        log.warn("Unmet dependency in "+p, d+" "+deps[d])
        depData = npm.config.get("parseable")
                ? ( npm.config.get("long")
                    ? path.resolve(data.path, "node_modules", d)
                    + ":"+d+"@"+JSON.stringify(depData)+":INVALID:MISSING"
                    : "" )
                : "─ \033[31;40mUNMET DEPENDENCY\033[0m "+d+" "+depData
      } else {
        if (npm.config.get("parseable")) {
          depData = path.resolve(data.path, "node_modules", d)
                  + (npm.config.get("long")
                    ? ":" + d + "@" + JSON.stringify(depData)
                    + ":" // no realpath resolved
                    + ":MAXDEPTH"
                    : "")
        } else {
          depData = "─ "+d+"@'"+depData +"' (max depth reached)"
        }
      }
    }
    var c = i === depLast ? "└─" : "├─"
    makePretty(depData, long, dir, childPref + c, list)
  })
  if (top && list.length === 1 && !data._id) {
    if (!npm.config.get("parseable")) {
      list.push("(empty)")
    } else if (npm.config.get("long")) list[0] += ":EMPTY"
  }
  return list.filter(function (l) { return l && l.trim() })
}

function ugly (data) {
  if (typeof data === "string") {
    return data
  }
  if (!npm.config.get("long")) return data.path

  return data.path
       + ":" + (data._id || "")
       + ":" + (data.realPath !== data.path ? data.realPath : "")
       + (data.extraneous ? ":EXTRANEOUS" : "")
       + (data.invalid ? ":INVALID" : "")
}

function format (data, long, prefix, dir) {
  if (npm.config.get("parseable")) return ugly(data)
  if (typeof data === "string") {
    return prefix + data
  }
//  console.log([data.path, dir], "relativize")
  var depLen = Object.keys(data.dependencies).length
    , space = prefix.split("├─").join("│ ")
                    .split("└─").join("  ")
            + (depLen ? "" : " ")
    , rel = relativize(data.path || "", dir)
    , l = prefix
        + (rel === "." ? "" : depLen ? "┬ " : "─ ")
        + (data._id ? data._id + " " : "")
        + (data.link ? "-> " + data.link : "") + ""
        + (rel === "." && !(long && data._id) ? dir : "")
  if (data.invalid) {
    if (data.realName !== data.name) l += " ("+data.realName+")"
    l += " \033[31;40minvalid\033[0m"
  }
  if (data.extraneous && rel !== ".") {
    l += " \033[32;40mextraneous\033[0m"
  }
  if (!long || !data._id) return l
  var extras = []
  if (rel !== ".") extras.push(rel)
  else extras.push(dir)
  if (data.description) extras.push(data.description)
  if (data.repository) extras.push(data.repository.url)
  if (data.homepage) extras.push(data.homepage)
  extras = extras.filter(function (e) { return e })
  var lastExtra = !depLen && extras.length - 1
  l += extras.map(function (e, i) {
    var indent = !depLen ? " " : "│ "
    return "\n" + space + indent + e
  }).join("")
  return l
}
