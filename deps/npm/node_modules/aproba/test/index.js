"use strict"
var test = require("tap").test
var validate = require("../index.js")

function thrown (t, code, msg, todo) {
  validate("OSSF", arguments)
  try {
    todo()
    t.fail(msg)
  }
  catch (e) {
    t.is(e.code, code, msg + e.message)
  }
}

function notThrown (t, msg, todo) {
  validate("OSF", arguments)
  try {
    todo()
    t.pass(msg)
  }
  catch (e) {
    t.fail(msg+"\n"+e.stack)
  }
}

test("general", function (t) {
  t.plan(69)
  var values = {
    "A": [],
    "S": "test",
    "N": 123,
    "F": function () {},
    "O": {},
    "B": false,
    "E": new Error()
  }
  Object.keys(values).forEach(function (type) {
    Object.keys(values).forEach(function (contraType) {
      if (type === contraType) {
        notThrown(t, type + " matches " + contraType, function () {
          validate(type, [values[contraType]])
        })
      }
      else {
        thrown(t, "EINVALIDTYPE", type + " does not match " + contraType, function () {
          validate(type, [values[contraType]])
        })
      }
    })
    if (type === "E") {
      notThrown(t, "null is ok for E", function () {
        validate(type, [null])
      })
    }
    else {
      thrown(t, "EMISSINGARG", "null not ok for "+type, function () {
        validate(type, [null])
      })
    }
  })
  Object.keys(values).forEach(function (contraType) {
    notThrown(t, "* matches " + contraType, function () {
      validate("*", [values[contraType]])
    })
  })
  thrown(t, "EMISSINGARG", "not enough args", function () {
    validate("SNF", ["abc", 123])
  })
  thrown(t, "ETOOMANYARGS", "too many args", function () {
    validate("SNF", ["abc", 123, function () {}, true])
  })
  notThrown(t, "E matches null", function () {
    validate("E", [null])
  })
  notThrown(t, "E matches undefined", function () {
    validate("E", [undefined])
  })
  notThrown(t, "E w/ error requires nothing else", function () {
    validate("ESN", [new Error(), "foo"])
  })
  thrown(t, "EMISSINGARG", "E w/o error works as usual", function () {
    validate("ESN", [null, "foo"])
  })
})
