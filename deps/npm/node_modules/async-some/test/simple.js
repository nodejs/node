var test = require("tap").test

var some = require("../some.js")

test("some() doesn't find anything asynchronously", function (t) {
  some(["a", "b", "c", "d", "e", "f", "g"], predicate, function (error, match) {
    t.ifError(error, "ran successfully")

    t.notOk(match, "nothing to find, so nothing found")

    t.end()
  })

  function predicate(value, cb) {
    // dezalgo ensures it's safe to not do this, but just in case
    setTimeout(function () { cb(null, value > "j" && value) })
  }
})

test("some() doesn't find anything synchronously", function (t) {
  some(["a", "b", "c", "d", "e", "f", "g"], predicate, function (error, match) {
    t.ifError(error, "ran successfully")

    t.notOk(match, "nothing to find, so nothing found")

    t.end()
  })

  function predicate(value, cb) {
    cb(null, value > "j" && value)
  }
})

test("some() doesn't find anything asynchronously", function (t) {
  some(["a", "b", "c", "d", "e", "f", "g"], predicate, function (error, match) {
    t.ifError(error, "ran successfully")

    t.equals(match, "d", "found expected element")

    t.end()
  })

  function predicate(value, cb) {
    setTimeout(function () { cb(null, value > "c" && value) })
  }
})

test("some() doesn't find anything synchronously", function (t) {
  some(["a", "b", "c", "d", "e", "f", "g"], predicate, function (error, match) {
    t.ifError(error, "ran successfully")

    t.equals(match, "d", "found expected")

    t.end()
  })

  function predicate(value, cb) {
    cb(null, value > "c" && value)
  }
})
