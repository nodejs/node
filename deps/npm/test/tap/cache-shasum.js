var npm = require.resolve("../../")
var test = require("tap").test
var path = require("path")
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")
var mr = require("npm-registry-mock")
var common = require("../common-tap.js")
var cache = path.resolve(__dirname, "cache-shasum")
var spawn = require("child_process").spawn
var sha = require("sha")
var server

test("mock reg", function(t) {
  rimraf.sync(cache)
  mkdirp.sync(cache)
  mr(common.port, function (s) {
    server = s
    t.pass("ok")
    t.end()
  })
})

test("npm cache add request", function(t) {
  var c = spawn(process.execPath, [
    npm, "cache", "add", "request@2.27.0",
    "--cache=" + cache,
    "--registry=" + common.registry,
    "--loglevel=quiet"
  ])
  c.stderr.pipe(process.stderr)

  c.stdout.on("data", function(d) {
    t.fail("Should not get data on stdout: " + d)
  })

  c.on("close", function(code) {
    t.notOk(code, "exit ok")
    t.end()
  })
})

test("compare", function(t) {
  var d = path.resolve(__dirname, "cache-shasum/request")
  var p = path.resolve(d, "2.27.0/package.tgz")
  var r = require("./cache-shasum/localhost_1337/request/.cache.json")
  var rshasum = r.versions["2.27.0"].dist.shasum
  sha.get(p, function (er, pshasum) {
    if (er)
      throw er
    t.equal(pshasum, rshasum)
    t.end()
  })
})

test("cleanup", function(t) {
  server.close()
  rimraf.sync(cache)
  t.end()
})
