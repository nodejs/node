var common = require("../common-tap.js")
var test = require("tap").test
var http = require("http")
var server

test("should send referer http header", function (t) {
  var server = http.createServer(function (q, s) {
    t.equal(q.headers.referer, "install foo")
    s.statusCode = 404
    s.end(JSON.stringify({error: "whatever"}))
    this.close()
  }).listen(common.port, function () {
    var reg = "--registry=http://localhost:" + common.port
    var args = [ "install", "foo", reg ]
    common.npm(args, {}, function (er, code, so, se) {
      if (er) {
        throw er
      }
      // should not have ended nicely, since we returned an error
      t.ok(code)
      t.end()
    })
  })
})
