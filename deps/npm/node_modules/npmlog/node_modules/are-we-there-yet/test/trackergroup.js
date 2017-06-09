'use strict'
var test = require('tap').test
var TrackerGroup = require('../index.js').TrackerGroup
var testEvent = require('./lib/test-event.js')

test('TrackerGroup', function (t) {
  var name = 'test'

  var track = new TrackerGroup(name)
  t.is(track.completed(), 0, 'Nothing todo is 0 completion')
  testEvent(track, 'change', afterFinishEmpty)
  track.finish()
  var a, b
  function afterFinishEmpty (er, onChangeName, completion) {
    t.is(er, null, 'finishEmpty: on change event fired')
    t.is(onChangeName, name, 'finishEmpty: on change emits the correct name')
    t.is(completion, 1, 'finishEmpty: passed through completion was correct')
    t.is(track.completed(), 1, 'finishEmpty: Finishing an empty group actually finishes it')

    track = new TrackerGroup(name)
    a = track.newItem('a', 10, 1)
    b = track.newItem('b', 10, 1)
    t.is(track.completed(), 0, 'Initially empty')
    testEvent(track, 'change', afterCompleteWork)
    a.completeWork(5)
  }
  function afterCompleteWork (er, onChangeName, completion) {
    t.is(er, null, 'on change event fired')
    t.is(onChangeName, 'a', 'on change emits the correct name')
    t.is(completion, 0.25, 'Complete half of one is a quarter overall')
    t.is(track.completed(), 0.25, 'Complete half of one is a quarter overall')
    testEvent(track, 'change', afterFinishAll)
    track.finish()
  }
  function afterFinishAll (er, onChangeName, completion) {
    t.is(er, null, 'finishAll: on change event fired')
    t.is(onChangeName, name, 'finishAll: on change emits the correct name')
    t.is(completion, 1, 'Finishing everything ')
    t.is(track.completed(), 1, 'Finishing everything ')

    track = new TrackerGroup(name)
    a = track.newItem('a', 10, 2)
    b = track.newItem('b', 10, 1)
    t.is(track.completed(), 0, 'weighted: Initially empty')
    testEvent(track, 'change', afterWeightedCompleteWork)
    a.completeWork(5)
  }
  function afterWeightedCompleteWork (er, onChangeName, completion) {
    t.is(er, null, 'weighted: on change event fired')
    t.is(onChangeName, 'a', 'weighted: on change emits the correct name')
    t.is(Math.floor(completion * 100), 33, 'weighted: Complete half of double weighted')
    t.is(Math.floor(track.completed() * 100), 33, 'weighted: Complete half of double weighted')
    testEvent(track, 'change', afterWeightedFinishAll)
    track.finish()
  }
  function afterWeightedFinishAll (er, onChangeName, completion) {
    t.is(er, null, 'weightedFinishAll: on change event fired')
    t.is(onChangeName, name, 'weightedFinishAll: on change emits the correct name')
    t.is(completion, 1, 'weightedFinishaAll: Finishing everything ')
    t.is(track.completed(), 1, 'weightedFinishaAll: Finishing everything ')

    track = new TrackerGroup(name)
    a = track.newGroup('a', 10)
    b = track.newGroup('b', 10)
    var a1 = a.newItem('a.1', 10)
    a1.completeWork(5)
    t.is(track.completed(), 0.25, 'nested: Initially quarter done')
    testEvent(track, 'change', afterNestedComplete)
    b.finish()
  }
  function afterNestedComplete (er, onChangeName, completion) {
    t.is(er, null, 'nestedComplete: on change event fired')
    t.is(onChangeName, 'b', 'nestedComplete: on change emits the correct name')
    t.is(completion, 0.75, 'nestedComplete: Finishing everything ')
    t.is(track.completed(), 0.75, 'nestedComplete: Finishing everything ')
    t.end()
  }
})

test('cycles', function (t) {
  var track = new TrackerGroup('top')
  testCycle(track, track)
  var layer1 = track.newGroup('layer1')
  testCycle(layer1, track)
  t.end()

  function testCycle (addTo, toAdd) {
    try {
      addTo.addUnit(toAdd)
      t.fail(toAdd.name)
    } catch (ex) {
      console.log(ex)
      t.pass(toAdd.name)
    }
  }
})
