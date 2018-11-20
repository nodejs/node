var common = require("../common-tap.js")
var test = require("tap").test

test("cache add", function (t) {
  common.npm(["cache", "add", "superfoo"], {}, function (er, c, so, se) {
    if (er) throw er
    t.ok(c, "got non-zero exit code")
    t.equal(so, "", "nothing printed to stdout")
    t.similar(se, /404 Not Found: superfoo/, "got expected error")
    t.end()
  })
})
