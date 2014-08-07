var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

tap.test("basic request", function (t) {
  server.expect("/-/all", function (req, res) {
    res.json([])
  })

  client.get("http://localhost:1337/-/all", null, function (er) {
    t.notOk(er, "no error")
    t.end()
  })
})
