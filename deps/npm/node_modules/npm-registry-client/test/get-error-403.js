var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")

tap.test("get fails with 403", function (t) {
  server.expect("/habanero", function (req, res) {
    t.equal(req.method, "GET", "got expected method")

    res.writeHead(403)
    res.end("{\"error\":\"get that cat out of the toilet that's gross omg\"}")
  })

  var client = common.freshClient()
  client.config.retry.minTimeout = 100
  client.get(
    "http://localhost:1337/habanero",
    {},
    function (er) {
      t.ok(er, "failed as expected")

      t.equal(er.statusCode, 403, "status code was attached as expected")
      t.equal(er.code, "E403", "error code was formatted as expected")
      t.equal(
        er.message,
        "get that cat out of the toilet that's gross omg : habanero",
        "got error message"
      )

      t.end()
    }
  )
})
