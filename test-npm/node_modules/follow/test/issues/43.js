var tap = require('tap')
var test = tap.test
var util = require('util')

var lib = require('../../lib')
var couch = require('../couch')
var follow = require('../../api')

couch.setup(test)

test('Issue #43', function(t) {
  var changes = 0

  var feed = follow({'db':couch.DB, 'inactivity_ms':couch.rtt()*3}, on_change)
  feed.on('stop', on_stop)
  feed.on('error', on_err)

  function on_err(er) {
    t.false(er, 'Error event should never fire')
    t.end()
  }

  function on_change(er, change) {
    changes += 1
    this.stop()
  }

  function on_stop(er) {
    t.false(er, 'No problem stopping')
    t.equal(changes, 1, 'Only one change seen since stop was called in its callback')
    t.end()
  }
})
