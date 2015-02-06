var test = require("tap").test
var path = require("path")
var fs = require("fs")
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")
var mr = require("npm-registry-mock")
var common = require("../common-tap.js")
var cache = path.resolve(__dirname, "cache-shasum-fork", "CACHE")
var cwd = path.resolve(__dirname, "cache-shasum-fork", "CWD")
var server

// Test for https://github.com/npm/npm/issues/3265

test("mock reg", function (t) {
  rimraf.sync(cache)
  mkdirp.sync(cache)
  rimraf.sync(cwd)
  mkdirp.sync(path.join(cwd, "node_modules"))
  mr({port : common.port}, function (er, s) {
    server = s
    t.pass("ok")
    t.end()
  })
})

test("npm cache - install from fork", function (t) {
  // Install from a tarball that thinks it is underscore@1.5.1
  // (but is actually a fork)
  var forkPath = path.resolve(
    __dirname, "cache-shasum-fork", "underscore-1.5.1.tgz")
  common.npm(["install", forkPath], {
      cwd: cwd,
      env: {
        "npm_config_cache"    : cache,
        "npm_config_registry" : common.registry,
        "npm_config_loglevel" : "silent"
      }
  }, function (err, code, stdout, stderr) {
    t.ifErr(err, "install finished without error")
    t.notOk(stderr, "Should not get data on stderr: " + stderr)
    t.equal(code, 0, "install finished successfully")

    t.equal(stdout, "underscore@1.5.1 node_modules/underscore\n")
    var index = fs.readFileSync(
      path.join(cwd, "node_modules", "underscore", "index.js"),
      "utf8"
    )
    t.equal(index, 'console.log("This is the fork");\n\n')
    t.end()
  })
})

test("npm cache - install from origin", function (t) {
  // Now install the real 1.5.1.
  rimraf.sync(path.join(cwd, "node_modules"))
  mkdirp.sync(path.join(cwd, "node_modules"))
  common.npm(["install", "underscore"], {
      cwd: cwd,
      env: {
        "npm_config_cache"    : cache,
        "npm_config_registry" : common.registry,
        "npm_config_loglevel" : "silent"
      }
  }, function (err, code, stdout, stderr) {
    t.ifErr(err, "install finished without error")
    t.equal(code, 0, "install finished successfully")
    t.notOk(stderr, "Should not get data on stderr: " + stderr)
    t.equal(stdout, "underscore@1.5.1 node_modules/underscore\n")
    var index = fs.readFileSync(
      path.join(cwd, "node_modules", "underscore", "index.js"),
      "utf8"
    )
    t.equal(index, "module.exports = require('./underscore');\n")
    t.end()
  })
})

test("cleanup", function (t) {
  server.close()
  rimraf.sync(cache)
  rimraf.sync(cwd)
  t.end()
})
