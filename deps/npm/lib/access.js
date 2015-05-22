var assert = require("assert")
var resolve = require("path").resolve
var url = require("url")

var log = require("npmlog")
var readPackageJson = require("read-package-json")

var mapToRegistry = require("./utils/map-to-registry.js")
var npa = require("npm-package-arg")
var npm = require("./npm.js")

module.exports = access

access.usage = "npm access public [<package>]"
             + "\nnpm access restricted [<package>]"
             + "\nnpm access add <read-only|read-write> <entity> [<package>]"
             + "\nnpm access rm <entity> [<package>]"
             + "\nnpm access ls [<package>]"
             + "\nnpm access edit [<package>]"

access.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length === 2) {
    return cb(null, ["public", "restricted", "add", "rm", "ls", "edit"])
  }

  switch (argv[2]) {
    case "public":
    case "restricted":
    case "ls":
    case "edit":
      return cb(new Error("unimplemented: packages you can change"))
    case "add":
      if (argv.length === 3) return cb(null, ["read-only", "read-write"])

      return cb(new Error("unimplemented: entities and packages"))
    case "rm":
      return cb(new Error("unimplemented: entities and packages"))
    default:
      return cb(new Error(argv[2]+" not recognized"))
  }
}

function access (args, cb) {
  var cmd = args.shift()
  switch (cmd) {
    case "public": case "restricted": return changeAccess(args, cmd, cb)
    case "add": case "set": return add(args, cb)
    case "rm": case "del": case "clear": return rm(args, cb)
    case "list": case "sl": case "ls": return ls(args, cb)
    case "edit": case "ed": return edit(args, cb)
    default: return cb("Usage:\n"+access.usage)
  }
}

function changeAccess (args, level, cb) {
  assert(Array.isArray(args), "changeAccess requires args be an array")
  assert(
    ["public", "restricted"].indexOf(level) !== -1,
    "access level must be either 'public' or 'restricted'"
  )
  assert(typeof cb === "function", "changeAccess requires a callback")

  var p = (args.shift() || "").trim()
  if (!p) return getCurrentPackage(level, cb)
  changeAccess_(p, level, cb)
}

function getCurrentPackage (level, cb) {
  var here = resolve(npm.prefix, "package.json")
  log.verbose("setPackageLevel", "here", here)

  readPackageJson(here, function (er, data) {
    if (er) return cb(er)

    if (!data.name) {
      return cb(new Error("Package must be named"))
    }

    changeAccess_(data.name, level, cb)
  })
}

function changeAccess_ (name, level, cb) {
  log.verbose("changeAccess", "name", name, "level", level)
  mapToRegistry(name, npm.config, function (er, uri, auth, base) {
    if (er) return cb(er)

    var data = npa(name)
    if (!data.scope) {
      var msg = "Sorry, you can't change the access level of unscoped packages."
      log.error("access", msg)
      return cb(new Error(msg))
    }

    // name must be scoped, so escape separator
    name = name.replace("/", "%2f")
    // FIXME: mapToRegistry still isn't generic enough SIGH
    uri = url.resolve(base, "-/package/"+name+"/access")
    var params = {
      level : level,
      auth : auth
    }

    npm.registry.access(uri, params, cb)
  })
}

function add (args, cb) {
  return cb(new Error("npm access add isn't implemented yet!"))
}

function rm (args, cb) {
  return cb(new Error("npm access rm isn't implemented yet!"))
}

function ls (args, cb) {
  return cb(new Error("npm access ls isn't implemented yet!"))
}

function edit (args, cb) {
  return cb(new Error("npm access edit isn't implemented yet!"))
}
