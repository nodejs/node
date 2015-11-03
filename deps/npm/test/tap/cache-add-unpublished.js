var common = require('../common-tap.js')
var test = require('tap').test

test("cache add", function (t) {
  common.npm(["cache", "add", "superfoo"], {}, function (er, c, so, se) {
    if (er) throw er
    t.ok(c)
    t.equal(so, "")
    t.similar(se, /'superfoo' is not in the npm registry./)
    t.end()
  })
})
