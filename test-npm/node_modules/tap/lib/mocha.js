// API surface inspired by Mocha,
// Copyright (c) TJ Holowaychuk <tj@vision-media.ca>
//
// Using these functions still outputs TAP, of course.
var stack = require('./stack.js')

exports.it = it
exports.describe = describe
exports.global = function () {
  Object.keys(exports).forEach(function (g) {
    global[g] = exports[g]
  })
}

var t = require('./root.js')

function it (name, fn) {
  var c = t.current()
  var at = stack.at(it)

  if (fn && fn.length) {
    c.test(name, { at: at }, function (tt) {
      return fn(function (err) {
        if (err) tt.threw(err)
        else tt.end()
      })
    })
  } else {
    c.doesNotThrow(fn, name || 'unnamed test', { at: at })
  }
}

function describe (name, fn) {
  var c = t.current()
  var at = stack.at(describe)
  if (!fn) {
    c.test(name, { at: at })
  } else if (fn.length) {
    c.test(name, { at: at }, function (tt) {
      return fn(function (err) {
        if (err) tt.threw(err)
        else tt.end()
      })
    })
  } else {
    c.test(name, { at: at }, function (tt) {
      var ret = fn()
      if (ret && ret.then) {
        return ret
      } else {
        tt.end()
      }
    })
  }
}
