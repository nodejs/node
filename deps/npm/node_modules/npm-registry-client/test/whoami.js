var test = require("tap").test

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

function nop () {}

var WHOIAM = "wombat"
var TOKEN  = "not-bad-meaning-bad-but-bad-meaning-wombat"
var AUTH   = { token : TOKEN }
var PARAMS = { auth : AUTH }

test("whoami call contract", function (t) {
  t.throws(function () {
    client.whoami(undefined, AUTH, nop)
  }, "requires a URI")

  t.throws(function () {
    client.whoami([], AUTH, nop)
  }, "requires URI to be a string")

  t.throws(function () {
    client.whoami(common.registry, undefined, nop)
  }, "requires params object")

  t.throws(function () {
    client.whoami(common.registry, "", nop)
  }, "params must be object")

  t.throws(function () {
    client.whoami(common.registry, AUTH, undefined)
  }, "requires callback")

  t.throws(function () {
    client.whoami(common.registry, AUTH, "callback")
  }, "callback must be function")

  t.throws(
    function () {
      var params = {}
      client.whoami(common.registry, params, nop)
    },
    { name : "AssertionError", message : "must pass auth to whoami" },
    "must pass auth to whoami"
  )

  t.end()
})

test("whoami", function (t) {
  server.expect("GET", "/whoami", function (req, res) {
    t.equal(req.method, "GET")
    // only available for token-based auth for now
    t.equal(req.headers.authorization, "Bearer not-bad-meaning-bad-but-bad-meaning-wombat")

    res.json({username : WHOIAM})
  })

  client.whoami(common.registry, PARAMS, function (error, wombat) {
    t.ifError(error, "no errors")
    t.equal(wombat, WHOIAM, "im a wombat")

    t.end()
  })
})
