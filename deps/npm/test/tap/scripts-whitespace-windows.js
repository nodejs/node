var test = require("tap").test
var common = require("../common-tap")
var path = require("path")
var pkg = path.resolve(__dirname, "scripts-whitespace-windows")
var tmp = path.resolve(pkg, "tmp")
var cache = path.resolve(pkg, "cache")
var modules = path.resolve(pkg, "node_modules")
var dep = path.resolve(pkg, "dep")

var mkdirp = require("mkdirp")
var rimraf = require("rimraf")

test("setup", function (t) {
  cleanup()
  mkdirp.sync(cache)
  mkdirp.sync(tmp)

  common.npm(["i", dep], {
    cwd: pkg,
    env: {
      "npm_config_cache": cache,
      "npm_config_tmp": tmp,
      "npm_config_prefix": pkg,
      "npm_config_global": "false"
    }
  }, function (err, code, stdout, stderr) {
    t.ifErr(err, "npm i " + dep + " finished without error")
    t.equal(code, 0, "npm i " + dep + " exited ok")
    t.notOk(stderr, "no output stderr")
    t.end()
  })
})

test("test", function (t) {
  common.npm(["run", "foo"], { cwd: pkg }, function (err, code, stdout, stderr) {
    t.ifErr(err, "npm run finished without error")
    t.equal(code, 0, "npm run exited ok")
    t.notOk(stderr, "no output stderr: ", stderr)
    stdout = stdout.trim()
    t.ok(/npm-test-fine/.test(stdout))
    t.end()
  })
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})

function cleanup() {
  rimraf.sync(cache)
  rimraf.sync(tmp)
  rimraf.sync(modules)
}
