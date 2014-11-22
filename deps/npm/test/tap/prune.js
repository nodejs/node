var test = require("tap").test
var common = require("../common-tap")
var fs = require("fs")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")
var env = process.env
var path = require("path")

var pkg = path.resolve(__dirname, "prune")
var cache = path.resolve(pkg, "cache")
var nodeModules = path.resolve(pkg, "node_modules")

var EXEC_OPTS = { cwd: pkg, env: env }
EXEC_OPTS.env.npm_config_depth = "Infinity"

var server

test("reg mock", function (t) {
  mr(common.port, function (s) {
    server = s
    t.pass("registry mock started")
    t.end()
  })
})

function cleanup () {
  rimraf.sync(cache)
  rimraf.sync(nodeModules)
}

test("setup", function (t) {
  cleanup()
  t.pass("setup")
  t.end()
})

test("npm install", function (t) {
  common.npm([
    "install",
    "--cache", cache,
    "--registry", common.registry,
    "--loglevel", "silent",
    "--production", "false"
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifErr(err, "install finished successfully")
    t.notOk(code, "exit ok")
    t.notOk(stderr, "Should not get data on stderr: " + stderr)
    t.end()
  })
})

test("npm install test-package", function (t) {
  common.npm([
    "install", "test-package",
    "--cache", cache,
    "--registry", common.registry,
    "--loglevel", "silent",
    "--production", "false"
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifErr(err, "install finished successfully")
    t.notOk(code, "exit ok")
    t.notOk(stderr, "Should not get data on stderr: " + stderr)
    t.end()
  })
})

test("verify installs", function (t) {
  var dirs = fs.readdirSync(pkg + "/node_modules").sort()
  t.same(dirs, [ "test-package", "mkdirp", "underscore" ].sort())
  t.end()
})

test("npm prune", function (t) {
  common.npm([
    "prune",
    "--loglevel", "silent",
    "--production", "false"
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifErr(err, "prune finished successfully")
    t.notOk(code, "exit ok")
    t.notOk(stderr, "Should not get data on stderr: " + stderr)
    t.end()
  })
})

test("verify installs", function (t) {
  var dirs = fs.readdirSync(pkg + "/node_modules").sort()
  t.same(dirs, [ "mkdirp", "underscore" ])
  t.end()
})

test("npm prune", function (t) {
  common.npm([
    "prune",
    "--loglevel", "silent",
    "--production"
  ], EXEC_OPTS, function (err, code, stderr) {
    t.ifErr(err, "prune finished successfully")
    t.notOk(code, "exit ok")
    t.equal(stderr, "unbuild mkdirp@0.3.5\n")
    t.end()
  })
})

test("verify installs", function (t) {
  var dirs = fs.readdirSync(pkg + "/node_modules").sort()
  t.same(dirs, [ "underscore" ])
  t.end()
})

test("cleanup", function (t) {
  server.close()
  cleanup()
  t.pass("cleaned up")
  t.end()
})
