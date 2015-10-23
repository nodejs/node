var common = require("../common-tap.js")
var test = require("tap").test
var http = require("http")

test("should send referer http header", function (t) {
  http.createServer(function (q, s) {
    t.equal(q.headers.referer, "install foo")
    s.statusCode = 404
    s.end(JSON.stringify({error: "whatever"}))
    this.close()
  }).listen(common.port, function () {
    var reg = "http://localhost:" + common.port
    var args = [ "install", "foo", "--registry", reg ]
    common.npm(args, {}, function (er, code) {
      if (er) {
        throw er
      }
      // should not have ended nicely, since we returned an error
      t.ok(code)
      t.end()
    })
  })
})
