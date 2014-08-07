var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient({
  username      : "othiym23",
  password      : "password",
  email         : "ogd@aoaioxxysz.net",
  _auth         : new Buffer("username  : password").toString("base64"),
  "always-auth" : true
})

var cache = require("./fixtures/underscore/cache.json")

var REV = "/-rev/72-47f2986bfd8e8b55068b204588bbf484"
var VERSION = "1.3.2"

tap.test("unpublish a package", function (t) {
  server.expect("GET", "/underscore?write=true", function (req, res) {
    t.equal(req.method, "GET")

    res.json(cache)
  })

  server.expect("PUT", "/underscore" + REV, function (req, res) {
    t.equal(req.method, "PUT")

    var b = ""
    req.setEncoding("utf-8")
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      var updated = JSON.parse(b)
      t.notOk(updated.versions[VERSION])
    })

    res.json(cache)
  })

  server.expect("GET", "/underscore", function (req, res) {
    t.equal(req.method, "GET")

    res.json(cache)
  })

  server.expect("DELETE", "/underscore/-/underscore-1.3.2.tgz" + REV, function (req, res) {
    t.equal(req.method, "DELETE")

    res.json({unpublished:true})
  })

  client.unpublish("http://localhost:1337/underscore", VERSION, function (error) {
    t.notOk(error, "no errors")

    t.end()
  })
})
