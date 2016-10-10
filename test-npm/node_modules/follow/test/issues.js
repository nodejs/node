var tap = require('tap')
  , test = tap.test
  , util = require('util')
  , request = require('request')
  , traceback = require('traceback')

var lib = require('../lib')
  , couch = require('./couch')
  , follow = require('../api')

couch.setup(test)

test('Issue #5', function(t) {
  var saw = { loops:0, seqs:{} }

  var saw_change = false
  // -2 means I want to see the last change.
  var feed = follow({'db':couch.DB, since:-2}, function(er, change) {
    t.equal(change.seq, 3, 'Got the latest change, 3')
    t.false(saw_change, 'Only one callback run for since=-2 (assuming no subsequent change')
    saw_change = true

    process.nextTick(function() { feed.stop() })
    feed.on('stop', function() {
      // Test using since=-1 (AKA since="now").
      follow({'db':couch.DB, since:'now'}, function(er, change) {
        t.equal(change.seq, 4, 'Only get changes made after starting the feed')
        t.equal(change.id, "You're in now, now", 'Got the subsequent change')

        this.stop()
        t.end()
      })

      // Let that follower settle in, then send it something
      setTimeout(function() {
        var doc = { _id:"You're in now, now", movie:"Spaceballs" }
        request.post({uri:couch.DB, json:doc}, function(er) {
          if(er) throw er
        })
      }, couch.rtt())
    })
  })
})

couch.setup(test) // Back to the expected documents

test('Issue #6', function(t) {
  // When we see change 1, delete the database. The rest should still come in, then the error indicating deletion.
  var saw = { seqs:{}, redid:false, redo_err:null }

  follow(couch.DB, function(er, change) {
    if(!er) {
      saw.seqs[change.seq] = true
      t.notOk(change.last_seq, 'Change '+change.seq+' ha no .last_seq')
      if(change.seq == 1) {
        couch.redo(function(er) {
          saw.redid = true
          saw.redo_err = er
        })
      }
    }

    else setTimeout(function() {
      // Give the redo time to finish, then confirm that everything happened as expected.
      // Hopefully this error indicates the database was deleted.
      t.ok(er.message.match(/deleted .* 3$/), 'Got delete error after change 3')
      t.ok(er.deleted, 'Error object indicates database deletion')
      t.equal(er.last_seq, 3, 'Error object indicates the last change number')

      t.ok(saw.seqs[1], 'Change 1 was processed')
      t.ok(saw.seqs[2], 'Change 2 was processed')
      t.ok(saw.seqs[3], 'Change 3 was processed')
      t.ok(saw.redid, 'The redo function ran')
      t.false(saw.redo_err, 'No problem redoing the database')

      return t.end()
    }, couch.rtt() * 2)
  })
})

test('Issue #8', function(t) {
  var timeouts = timeout_tracker()

  // Detect inappropriate timeouts after the run.
  var runs = {'set':false, 'clear':false}
  function badSetT() {
    runs.set = true
    return setTimeout.apply(this, arguments)
  }

  function badClearT() {
    runs.clear = true
    return clearTimeout.apply(this, arguments)
  }

  follow(couch.DB, function(er, change) {
    t.false(er, 'Got a feed')
    t.equal(change.seq, 1, 'Handler only runs for one change')

    this.on('stop', check_timeouts)
    this.stop()

    function check_timeouts() {
      t.equal(timeouts().length, 0, 'No timeouts by the time stop fires')

      lib.timeouts(badSetT, badClearT)

      // And give it a moment to try something bad.
      setTimeout(final_timeout_check, 250)
      function final_timeout_check() {
        t.equal(timeouts().length, 0, 'No lingering timeouts after teardown: ' + tims(timeouts()))
        t.false(runs.set, 'No more setTimeouts ran')
        t.false(runs.clear, 'No more clearTimeouts ran')

        t.end()
      }
    }
  })
})

test('Issue #9', function(t) {
  var timeouts = timeout_tracker()

  follow({db:couch.DB, inactivity_ms:30000}, function(er, change) {
    if(change.seq == 1)
      return // Let it run through once, just for fun.

    t.equal(change.seq, 2, 'The second change will be the last')
    this.stop()

    setTimeout(check_inactivity_timer, 250)
    function check_inactivity_timer() {
      t.equal(timeouts().length, 0, 'No lingering timeouts after teardown: ' + tims(timeouts()))
      timeouts().forEach(function(id) { clearTimeout(id) })
      t.end()
    }
  })
})

//
// Utilities
//

function timeout_tracker() {
  // Return an array tracking in-flight timeouts.
  var timeouts = []
  var set_num = 0

  lib.timeouts(set, clear)
  return function() { return timeouts }

  function set() {
    var result = setTimeout.apply(this, arguments)

    var caller = traceback()[2]
    set_num += 1
    result.caller = '('+set_num+') ' + (caller.method || caller.name || '<a>') + ' in ' + caller.file + ':' + caller.line
    //console.error('setTimeout: ' + result.caller)

    timeouts.push(result)
    //console.error('inflight ('+timeouts.length+'): ' + tims(timeouts))
    return result
  }

  function clear(id) {
    //var caller = traceback()[2]
    //caller = (caller.method || caller.name || '<a>') + ' in ' + caller.file + ':' + caller.line
    //console.error('clearTimeout: ' + (id && id.caller) + ' <- ' + caller)

    timeouts = timeouts.filter(function(element) { return element !== id })
    //console.error('inflight ('+timeouts.length+'): ' + tims(timeouts))
    return clearTimeout.apply(this, arguments)
  }
}

function tims(arr) {
  return JSON.stringify(arr.map(function(timer) { return timer.caller }))
}
