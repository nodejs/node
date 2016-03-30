'use strict'

var test = require('tap').test
var Progress = require('are-we-there-yet')
var log = require('../log.js')

var actions = []
log.gauge = {
  enable: function () {
    actions.push(['enable'])
  },
  disable: function () {
    actions.push(['disable'])
  },
  hide: function () {
    actions.push(['hide'])
  },
  show: function (name, completed) {
    actions.push(['show', name, completed])
  },
  pulse: function (name) {
    actions.push(['pulse', name])
  }
}

function didActions(t, msg, output) {
  var tests = []
  for (var ii = 0; ii < output.length; ++ ii) {
    for (var jj = 0; jj < output[ii].length; ++ jj) {
      tests.push({cmd: ii, arg: jj})
    }
  }
  t.is(actions.length, output.length, msg)
  tests.forEach(function (test) {
    t.is(actions[test.cmd] ? actions[test.cmd][test.arg] : null, 
         output[test.cmd][test.arg],
         msg + ': ' + output[test.cmd] + (test.arg ? ' arg #'+test.arg : ''))
  })
  actions = []
}

function resetTracker() {
  log.disableProgress()
  log.tracker = new Progress.TrackerGroup()
  log.enableProgress()
  actions = []
}

test('enableProgress', function (t) {
  t.plan(6)
  resetTracker()
  log.disableProgress()
  actions = []
  log.enableProgress()
  didActions(t, 'enableProgress', [ [ 'enable' ], [ 'show', undefined, 0 ] ])
  log.enableProgress()
  didActions(t, 'enableProgress again', [])
})

test('disableProgress', function (t) {
  t.plan(4)
  resetTracker()
  log.disableProgress()
  didActions(t, 'disableProgress', [ [ 'hide' ], [ 'disable' ] ])
  log.disableProgress()
  didActions(t, 'disableProgress again', [])
})

test('showProgress', function (t) {
  t.plan(5)
  resetTracker()
  log.disableProgress()
  actions = []
  log.showProgress('foo')
  didActions(t, 'showProgress disabled', [])
  log.enableProgress()
  actions = []
  log.showProgress('foo')
  didActions(t, 'showProgress', [ [ 'show', 'foo', 0 ] ])
})

test('clearProgress', function (t) {
  t.plan(3)
  resetTracker()
  log.clearProgress()
  didActions(t, 'clearProgress', [ [ 'hide' ] ])
  log.disableProgress()
  actions = []
  log.clearProgress()
  didActions(t, 'clearProgress disabled', [ ])
})

test("newItem", function (t) {
  t.plan(12)
  resetTracker()
  actions = []
  var a = log.newItem("test", 10)
  didActions(t, "newItem", [ [ 'show', 'test', 0 ] ])
  a.completeWork(5)
  didActions(t, "newItem:completeWork", [ [ 'show', 'test', 0.5 ] ])
  a.finish()
  didActions(t, "newItem:finish", [ [ 'show', 'test', 1 ] ])
})

// test that log objects proxy through. And test that completion status filters up
test("newGroup", function (t) {
  t.plan(23)
  resetTracker()
  var a = log.newGroup("newGroup")
  didActions(t, 'newGroup', [[ 'show', 'newGroup', 0 ]])
  a.warn("test", "this is a test")
  didActions(t, "newGroup:warn", [ [ 'pulse', 'test' ], [ 'hide' ], [ 'show', undefined, 0 ] ])
  var b = a.newItem("newGroup2", 10)
  didActions(t, "newGroup:newItem", [ [ 'show', 'newGroup2', 0 ] ])
  b.completeWork(5)
  didActions(t, "newGroup:completeWork", [ [ 'show', 'newGroup2', 0.5] ])
  a.finish()
  didActions(t, "newGroup:finish", [ [ 'show', 'newGroup', 1 ] ])
})

test("newStream", function (t) {
  t.plan(13)
  resetTracker()
  var a = log.newStream("newStream", 10)
  didActions(t, "newStream", [ [ 'show', 'newStream', 0 ] ])
  a.write("abcde")
  didActions(t, "newStream", [ [ 'show', 'newStream', 0.5 ] ])
  a.write("fghij")
  didActions(t, "newStream", [ [ 'show', 'newStream', 1 ] ])
  t.is(log.tracker.completed(), 1, "Overall completion")
})
