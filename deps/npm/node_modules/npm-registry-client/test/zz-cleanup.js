var tap = require("tap")
var rimraf = require("rimraf")

tap.test("teardown", function (t) {
  rimraf(__dirname + "/fixtures/cache", function (er) {
    if (er) throw er
    t.pass("cache cleaned")
    t.end()
  })
})
