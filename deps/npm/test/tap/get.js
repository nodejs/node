var common = require("../common-tap.js")
var test = require("tap").test
var cacheFile = require("npm-cache-filename")
var npm = require("../../")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var path = require("path")
var mr = require("npm-registry-mock")
var fs = require("graceful-fs")

function nop () {}

var URI      = "https://npm.registry:8043/rewrite"
var TIMEOUT  = 3600
var FOLLOW   = false
var STALE_OK = true
var TOKEN    = "lolbutts"
var AUTH     = {
  token   : TOKEN
}
var PARAMS   = {
  timeout : TIMEOUT,
  follow  : FOLLOW,
  staleOk : STALE_OK,
  auth    : AUTH
}
var PKG_DIR = path.resolve(__dirname, "get-basic")
var CACHE_DIR = path.resolve(PKG_DIR, "cache")
var BIGCO_SAMPLE = {
  name : "@bigco/sample",
  version : "1.2.3"
}

// mock server reference
var server

var mocks = {
  "get": {
    "/@bigco%2fsample/1.2.3" : [200, JSON.stringify(BIGCO_SAMPLE)]
  }
}

var mapper = cacheFile(CACHE_DIR)

function getCachePath (uri) {
  return path.join(mapper(uri), ".cache.json")
}

test("setup", function (t) {
  mkdirp.sync(CACHE_DIR)

  mr({port: common.port, mocks: mocks}, function (s) {
    npm.load({cache: CACHE_DIR, registry: common.registry}, function (err) {
      t.ifError(err)
      server = s
      t.end()
    })
  })
})

test("get call contract", function (t) {
  t.throws(function () {
    npm.registry.get(undefined, PARAMS, nop)
  }, "requires a URI")

  t.throws(function () {
    npm.registry.get([], PARAMS, nop)
  }, "requires URI to be a string")

  t.throws(function () {
    npm.registry.get(URI, undefined, nop)
  }, "requires params object")

  t.throws(function () {
    npm.registry.get(URI, "", nop)
  }, "params must be object")

  t.throws(function () {
    npm.registry.get(URI, PARAMS, undefined)
  }, "requires callback")

  t.throws(function () {
    npm.registry.get(URI, PARAMS, "callback")
  }, "callback must be function")

  t.end()
})

test("basic request", function (t) {
  t.plan(9)

  var versioned = common.registry + "/underscore/1.3.3"
  npm.registry.get(versioned, PARAMS, function (er, data) {
    t.ifError(er, "loaded specified version underscore data")
    t.equal(data.version, "1.3.3")
    fs.stat(getCachePath(versioned), function (er) {
      t.ifError(er, "underscore 1.3.3 cache data written")
    })
  })

  var rollup = common.registry + "/underscore"
  npm.registry.get(rollup, PARAMS, function (er, data) {
    t.ifError(er, "loaded all metadata")
    t.deepEqual(data.name, "underscore")
    fs.stat(getCachePath(rollup), function (er) {
      t.ifError(er, "underscore rollup cache data written")
    })
  })

  var scoped = common.registry + "/@bigco%2fsample/1.2.3"
  npm.registry.get(scoped, PARAMS, function (er, data) {
    t.ifError(er, "loaded all metadata")
    t.deepEqual(data.name, "@bigco/sample")
    fs.stat(getCachePath(scoped), function (er) {
      t.ifError(er, "scoped cache data written")
    })
  })
})

test("cleanup", function (t) {
  server.close()
  rimraf.sync(PKG_DIR)

  t.end()
})
