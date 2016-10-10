var BlockStream = require("../")
var tap = require("tap")


tap.test("don't pad, small writes", function (t) {
  var f = new BlockStream(16, { nopad: true })
  t.plan(1)

  f.on("data", function (c) {
    t.equal(c.toString(), "abc", "should get 'abc'")
  })

  f.on("end", function () { t.end() })

  f.write(new Buffer("a"))
  f.write(new Buffer("b"))
  f.write(new Buffer("c"))
  f.end()
})

tap.test("don't pad, exact write", function (t) {
  var f = new BlockStream(16, { nopad: true })
  t.plan(1)

  var first = true
  f.on("data", function (c) {
    if (first) {
      first = false
      t.equal(c.toString(), "abcdefghijklmnop", "first chunk")
    } else {
      t.fail("should only get one")
    }
  })

  f.on("end", function () { t.end() })

  f.end(new Buffer("abcdefghijklmnop"))
})

tap.test("don't pad, big write", function (t) {
  var f = new BlockStream(16, { nopad: true })
  t.plan(2)

  var first = true
  f.on("data", function (c) {
    if (first) {
      first = false
      t.equal(c.toString(), "abcdefghijklmnop", "first chunk")
    } else {
      t.equal(c.toString(), "q")
    }
  })

  f.on("end", function () { t.end() })

  f.end(new Buffer("abcdefghijklmnopq"))
})
