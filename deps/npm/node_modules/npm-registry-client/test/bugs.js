var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient({
  username      : "username",
  password      : "%1234@asdf%",
  email         : "ogd@aoaioxxysz.net",
  _auth         : new Buffer("username:%1234@asdf%").toString("base64"),
  "always-auth" : true
})

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
    t.notOk(error, "no errors")
    t.ok(info.url, "got the URL")
    t.ok(info.email, "got the email address")

    t.end()
  })
})

