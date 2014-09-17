var test = require("tap").test
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")
var common = require("../common-tap.js")
var path = require("path")
var fs = require("fs")
var dir = path.resolve(__dirname, "unpack-foreign-tarball")
var root = path.resolve(dir, "root")
var nm = path.resolve(root, "node_modules")
var cache = path.resolve(dir, "cache")
var tmp = path.resolve(dir, "tmp")
var pkg = path.resolve(nm, "npm-test-gitignore")

var env = {
  npm_config_cache: cache,
  npm_config_tmp: tmp
}

var conf = {
  env: env,
  cwd: root,
  stdio: [ "pipe", "pipe", 2 ]
}

test("npmignore only", function (t) {
  setup()
  var file = path.resolve(dir, "npmignore.tgz")
  common.npm(["install", file], conf, function (code, stdout, stderr) {
    verify(t, code, ["foo"])
  })
})

test("gitignore only", function (t) {
  setup()
  var file = path.resolve(dir, "gitignore.tgz")
  common.npm(["install", file], conf, function (code, stdout, stderr) {
    verify(t, code, ["foo"])
  })
})

test("gitignore and npmignore", function (t) {
  setup()
  var file = path.resolve(dir, "gitignore-and-npmignore.tgz")
  common.npm(["install", file], conf, function (code, stdout, stderr) {
    verify(t, code, ["foo", "bar"])
  })
})

test("gitignore and npmignore, not gzipped", function (t) {
  setup()
  var file = path.resolve(dir, "gitignore-and-npmignore.tar")
  common.npm(["install", file], conf, function (code, stdout, stderr) {
    verify(t, code, ["foo", "bar"])
  })
})

test("clean", function (t) {
  clean()
  t.end()
})

function verify (t, code, files) {
  if (code) {
    t.fail("exited with failure: " + code)
    return t.end()
  }
  var actual = fs.readdirSync(pkg).sort()
  var expect = files.concat([".npmignore", "package.json"]).sort()
  t.same(actual, expect)
  t.end()
}

function setup () {
  clean()
  mkdirp.sync(nm)
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
}

function clean () {
  rimraf.sync(root)
  rimraf.sync(cache)
  rimraf.sync(tmp)
}
