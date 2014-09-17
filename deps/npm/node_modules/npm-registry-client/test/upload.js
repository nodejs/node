var tap = require("tap")
var Readable = require("stream").Readable
var inherits = require("util").inherits

var common = require("./lib/common.js")
var server = require("./lib/server.js")

var cache = require("./fixtures/underscore/cache.json")

var client = common.freshClient({
  username      : "username",
  password      : "%1234@asdf%",
  email         : "ogd@aoaioxxysz.net",
  _auth         : new Buffer("username:%1234@asdf%").toString("base64"),
  "always-auth" : true
})

function OneA() {
  Readable.call(this)
  this.push("A")
  this.push(null)
}
inherits(OneA, Readable)

tap.test("unpublish a package", function (t) {
  server.expect("PUT", "/underscore", function (req, res) {
    t.equal(req.method, "PUT")

    res.json(cache)
  })

  client.upload("http://localhost:1337/underscore", new OneA(), "daedabeefa", true, function (error) {
    t.notOk(error, "no errors")

    t.end()
  })
})
