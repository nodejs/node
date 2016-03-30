'use strict'
var test = require('tap').test
var Tracker = require('../index.js').Tracker

var testEvent = require('./lib/test-event.js')

var name = 'test'

test('initialization', function (t) {
  var simple = new Tracker(name)

  t.is(simple.completed(), 0, 'Nothing todo is 0 completion')
  t.done()
})

var track
var todo = 100
test('completion', function (t) {
  track = new Tracker(name, todo)
  t.is(track.completed(), 0, 'Nothing done is 0 completion')

  testEvent(track, 'change', afterCompleteWork)

  track.completeWork(todo)
  t.is(track.completed(), 1, 'completeWork: 100% completed')

  function afterCompleteWork (er, onChangeName) {
    t.is(er, null, 'completeWork: on change event fired')
    t.is(onChangeName, name, 'completeWork: on change emits the correct name')
    t.done()
  }
})

test('add more work', function (t) {
  testEvent(track, 'change', afterAddWork)
  track.addWork(todo)
  t.is(track.completed(), 0.5, 'addWork: 50% completed')
  function afterAddWork (er, onChangeName) {
    t.is(er, null, 'addWork: on change event fired')
    t.is(onChangeName, name, 'addWork: on change emits the correct name')
    t.done()
  }
})

test('complete more work', function (t) {
  track.completeWork(200)
  t.is(track.completed(), 1, 'completeWork: Over completion is still only 100% complete')
  t.done()
})

test('finish is always 100%', function (t) {
  var finishtest = new Tracker(name, todo)
  finishtest.completeWork(50)
  finishtest.finish()
  t.is(finishtest.completed(), 1, 'finish: Explicitly finishing moves to 100%')
  t.done()
})
