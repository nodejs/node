if (process.platform === "win32") {
  console.log("ok - symlinks are weird on windows, skip this test")
  return
}
var common = require("../common-tap.js")
var test = require("tap").test
var path = require("path")
var fs = require("fs")
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")

var root = path.resolve(__dirname, "ignore-install-link")
var pkg = path.resolve(root, "pkg")
var dep = path.resolve(root, "dep")
var target = path.resolve(pkg, "node_modules", "dep")
var cache = path.resolve(root, "cache")
var globalPath = path.resolve(root, "global")

var pkgj = { "name":"pkg", "version": "1.2.3"
           , "dependencies": { "dep": "1.2.3" } }
var depj = { "name": "dep", "version": "1.2.3" }

var myreg = require("http").createServer(function (q, s) {
  s.statusCode = 403
  s.end(JSON.stringify({"error":"forbidden"}) + "\n")
}).listen(common.port)

test("setup", function (t) {
  rimraf.sync(root)
  mkdirp.sync(root)
  mkdirp.sync(path.resolve(pkg, "node_modules"))
  mkdirp.sync(dep)
  mkdirp.sync(cache)
  mkdirp.sync(globalPath)
  fs.writeFileSync(path.resolve(pkg, "package.json"), JSON.stringify(pkgj))
  fs.writeFileSync(path.resolve(dep, "package.json"), JSON.stringify(depj))
  fs.symlinkSync(dep, target, "dir")
  t.end()
})

test("ignore install if package is linked", function (t) {
  common.npm(["install"], {
    cwd: pkg,
    env: {
      PATH: process.env.PATH || process.env.Path,
      HOME: process.env.HOME,
      "npm_config_prefix": globalPath,
      "npm_config_cache": cache,
      "npm_config_registry": common.registry,
      "npm_config_loglevel": "silent"
    },
    stdio: "inherit"
  }, function (er, code) {
    if (er) throw er
    t.equal(code, 0, "npm install exited with code")
    t.end()
  })
})

test("still a symlink", function (t) {
  t.equal(true, fs.lstatSync(target).isSymbolicLink())
  t.end()
})

test("cleanup", function (t) {
  rimraf.sync(root)
  myreg.close()
  t.end()
})
