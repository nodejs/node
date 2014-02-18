var test = require("tap").test
var fs = require("fs")
var node = process.execPath
var npm = require.resolve("../../bin/npm-cli.js")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")
var common = require("../common-tap.js")
var spawn = require("child_process").spawn

var pkg = __dirname + "/prune"

var server

test("reg mock", function (t) {
  mr(common.port, function (s) {
    server = s
    t.pass("registry mock started")
    t.end()
  })
})

test("npm install", function (t) {
  var c = spawn(node, [
    npm, "install",
    "--registry=" + common.registry,
    "--loglevel=silent",
    "--production=false"
  ], { cwd: pkg })
  c.stderr.on("data", function(d) {
    t.fail("Should not get data on stderr: " + d)
  })
  c.on("close", function(code) {
    t.notOk(code, "exit ok")
    t.end()
  })
})

test("npm install test-package", function (t) {
  var c = spawn(node, [
    npm, "install", "test-package",
    "--registry=" + common.registry,
    "--loglevel=silent",
    "--production=false"
  ], { cwd: pkg })
  c.stderr.on("data", function(d) {
    t.fail("Should not get data on stderr: " + d)
  })
  c.on("close", function(code) {
    t.notOk(code, "exit ok")
    t.end()
  })
})

test("verify installs", function (t) {
  var dirs = fs.readdirSync(pkg + "/node_modules").sort()
  t.same(dirs, [ "test-package", "mkdirp", "underscore" ].sort())
  t.end()
})

test("npm prune", function (t) {
  var c = spawn(node, [
    npm, "prune",
    "--loglevel=silent",
    "--production=false"
  ], { cwd: pkg })
  c.stderr.on("data", function(d) {
    t.fail("Should not get data on stderr: " + d)
  })
  c.on("close", function(code) {
    t.notOk(code, "exit ok")
    t.end()
  })
})

test("verify installs", function (t) {
  var dirs = fs.readdirSync(pkg + "/node_modules").sort()
  t.same(dirs, [ "mkdirp", "underscore" ])
  t.end()
})

test("npm prune", function (t) {
  var c = spawn(node, [
    npm, "prune",
    "--loglevel=silent",
    "--production"
  ], { cwd: pkg })
  c.stderr.on("data", function(d) {
    t.fail("Should not get data on stderr: " + d)
  })
  c.on("close", function(code) {
    t.notOk(code, "exit ok")
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
  rimraf.sync(pkg + "/node_modules")
  t.pass("cleaned up")
  t.end()
})
