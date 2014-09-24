var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

tap.test("get the URL for the bugs page on a package", function (t) {
  server.expect("GET", "/sample/latest", function (req, res) {
    t.equal(req.method, "GET")

    res.json({
      bugs : {
        url : "http://github.com/example/sample/issues",
        email : "sample@example.com"
      }
    })
  })

  client.bugs("http://localhost:1337/sample", function (error, info) {
    t.ifError(error)

    t.ok(info.url, "got the URL")
    t.ok(info.email, "got the email address")

    t.end()
  })
})

