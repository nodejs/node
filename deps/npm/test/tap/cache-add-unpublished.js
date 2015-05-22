var common = require("../common-tap.js")
var test = require("tap").test
var mr = require("npm-registry-mock")

test("cache add", function (t) {
  setup(function (er, s) {
    if (er) {
      throw er
    }
    common.npm([
      "cache",
      "add",
      "superfoo",
      "--registry=http://localhost:1337/"
    ], {}, function (er, c, so, se) {
      if (er) throw er
      t.ok(c, "got non-zero exit code")
      t.equal(so, "", "nothing printed to stdout")
      t.similar(se, /404 Not Found: superfoo/, "got expected error")
      s.close()
      t.end()
    })
  })
})

function setup (cb) {
  var s = require("http").createServer(function (req, res) {
    res.statusCode = 404
    res.end("{\"error\":\"not_found\"}\n")
  })
  s.listen(1337, function () {
    cb(null, s)
  })
}
