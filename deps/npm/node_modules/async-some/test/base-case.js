var test = require("tap").test

var some = require("../some.js")

test("some() array base case", function (t) {
  some([], failer, function (error, match) {
    t.ifError(error, "ran successfully")

    t.notOk(match, "nothing to find, so nothing found")

    t.end()
  })

  function failer(value, cb) {
    cb(new Error("test should never have been called"))
  }
})

test("some() arguments arraylike base case", function (t) {
  go()

  function go() {
    some(arguments, failer, function (error, match) {
      t.ifError(error, "ran successfully")

      t.notOk(match, "nothing to find, so nothing found")

      t.end()
    })

    function failer(value, cb) {
      cb(new Error("test should never have been called"))
    }
  }
})
