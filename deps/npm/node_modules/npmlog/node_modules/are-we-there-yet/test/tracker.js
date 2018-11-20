"use strict"
var test = require("tap").test
var Tracker = require("../index.js").Tracker

var timeoutError = new Error("timeout")
var testEvent = function (obj,event,next) {
  var timeout = setTimeout(function(){
    obj.removeListener(event, eventHandler)
    next(timeoutError)
  }, 10)
  var eventHandler = function () {
    var args = Array.prototype.slice.call(arguments)
    args.unshift(null)
    clearTimeout(timeout)
    next.apply(null, args)
  }
  obj.once(event, eventHandler)
}

test("Tracker", function (t) {
  t.plan(10)

  var name = "test"
  var track = new Tracker(name)

  t.is(track.completed(), 0, "Nothing todo is 0 completion")

  var todo = 100
  track = new Tracker(name, todo)
  t.is(track.completed(), 0, "Nothing done is 0 completion")

  testEvent(track, "change", afterCompleteWork)
  track.completeWork(100)
  function afterCompleteWork(er, onChangeName) {
    t.is(er, null, "completeWork: on change event fired")
    t.is(onChangeName, name, "completeWork: on change emits the correct name")
  }
  t.is(track.completed(), 1, "completeWork: 100% completed")

  testEvent(track, "change", afterAddWork)
  track.addWork(100)
  function afterAddWork(er, onChangeName) {
    t.is(er, null, "addWork: on change event fired")
    t.is(onChangeName, name, "addWork: on change emits the correct name")
  }
  t.is(track.completed(), 0.5, "addWork: 50% completed")


  track.completeWork(200)
  t.is(track.completed(), 1, "completeWork: Over completion is still only 100% complete")

  track = new Tracker(name, todo)
  track.completeWork(50)
  track.finish()
  t.is(track.completed(), 1, "finish: Explicitly finishing moves to 100%")
})
