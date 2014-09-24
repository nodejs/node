// verify that prepublish runs on pack and publish
var test = require("tap").test
var fs = require("graceful-fs")
var join = require("path").join
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var path = require("path")

var pkg      = join(__dirname, "scoped_package")
var manifest = join(pkg, "package.json")
var tmp      = join(pkg, "tmp")
var cache    = join(pkg, "cache")

var data = {
  name    : "@scope/generic-package",
  version : "90000.100001.5"
}

test("setup", function (t) {
  var n = 0

  mkdirp(pkg,   then())
  mkdirp(cache, then())
  mkdirp(tmp,   then())

  function then () {
    n++
    return function (er) {
      if (er) throw er
      if (--n === 0) next()
    }
  }

  function next () {
    fs.writeFile(manifest, JSON.stringify(data), "ascii", done)
  }

  function done (er) {
    if (er) throw er

    t.pass("setup done")
    t.end()
  }
})

test("test", function (t) {
  var spawn = require("child_process").spawn
  var node = process.execPath
  var npm = path.resolve(__dirname, "../../cli.js")
  var env = {
    "npm_config_cache"  : cache,
    "npm_config_tmp"    : tmp,
    "npm_config_prefix" : pkg,
    "npm_config_global" : "false"
  }

  for (var i in process.env) {
    if (!/^npm_config_/.test(i)) env[i] = process.env[i]
  }

  var child = spawn(node, [npm, "pack"], {cwd : pkg, env : env})

  child.stdout.setEncoding("utf8")
  child.stderr.on("data", onerr)
  child.stdout.on("data", ondata)
  child.on("close", onend)

  var c = "", e = ""
  function ondata (chunk) { c += chunk }
  function onerr  (chunk) { e += chunk }

  function onend () {
    if (e) {
      throw new Error("got stderr data: " + JSON.stringify("" + e))
    }
    c = c.trim()
    var regex = new RegExp("scope-generic-package-90000.100001.5.tgz", "ig")

    t.ok(c.match(regex), "found package")
    t.end()
  }
})

test("cleanup", function (t) {
  rimraf(pkg, function (er) {
    if (er) throw er

    t.pass("cleaned up")
    t.end()
  })
})
