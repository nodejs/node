var resolve = require("path").resolve
var createReadStream = require("graceful-fs").createReadStream
var readFileSync = require("graceful-fs").readFileSync

var test = require("tap").test
var concat = require("concat-stream")

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

var tgz = resolve(__dirname, "./fixtures/underscore/1.3.3/package.tgz")

function nop () {}

var URI      = "https://npm.registry:8043/rewrite"
var USERNAME = "username"
var PASSWORD = "hi"
var EMAIL    = "n@p.m"
var HEADERS  = {
  "npm-custom" : "lolbutts"
}
var AUTH     = {
  username : USERNAME,
  password : PASSWORD,
  email : EMAIL
}
var PARAMS   = {
  headers : HEADERS,
  auth    : AUTH
}

test("fetch call contract", function (t) {
  t.throws(function () {
    client.get(undefined, PARAMS, nop)
  }, "requires a URI")

  t.throws(function () {
    client.get([], PARAMS, nop)
  }, "requires URI to be a string")

  t.throws(function () {
    client.get(URI, undefined, nop)
  }, "requires params object")

  t.throws(function () {
    client.get(URI, "", nop)
  }, "params must be object")

  t.throws(function () {
    client.get(URI, PARAMS, undefined)
  }, "requires callback")

  t.throws(function () {
    client.get(URI, PARAMS, "callback")
  }, "callback must be function")

  t.end()
})

test("basic fetch", function (t) {
  server.expect("/underscore/-/underscore-1.3.3.tgz", function (req, res) {
    t.equal(req.method, "GET", "got expected method")

    res.writeHead(200, {
      "content-type"     : "application/x-tar",
      "content-encoding" : "gzip"
    })

    createReadStream(tgz).pipe(res)
  })

  var defaulted = {}
  client.fetch(
    "http://localhost:1337/underscore/-/underscore-1.3.3.tgz",
    defaulted,
    function (er, res) {
      t.ifError(er, "loaded successfully")

      var sink = concat(function (data) {
        t.deepEqual(data, readFileSync(tgz))
        t.end()
      })

      res.on("error", function (error) {
        t.ifError(error, "no errors on stream")
      })

      res.pipe(sink)
    }
  )
})
