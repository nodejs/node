var tap = require('tap')
  , test = tap.test
  , util = require('util')
  , request = require('request')

var couch = require('./couch')
  , follow = require('../api')


couch.setup(test)

test('Follow API', function(t) {
  var i = 0
    , saw = {}

  var feed = follow(couch.DB, function(er, change) {
    t.is(this, feed, 'Callback "this" value is the feed object')

    i += 1
    t.false(er, 'No error coming back from follow: ' + i)
    t.equal(change.seq, i, 'Change #'+i+' should have seq_id='+i)
    saw[change.id] = true

    if(i == 3) {
      t.ok(saw.doc_first, 'Got the first document')
      t.ok(saw.doc_second, 'Got the second document')
      t.ok(saw.doc_third , 'Got the third document')

      t.doesNotThrow(function() { feed.stop() }, 'No problem calling stop()')

      t.end()
    }
  })
})

test("Confirmation request behavior", function(t) {
  var feed = follow(couch.DB, function() {})

  var confirm_req = null
    , follow_req = null

  feed.on('confirm_request', function(req) { confirm_req = req })
  feed.on('query', function(req) { follow_req = req })

  setTimeout(check_req, couch.rtt() * 2)
  function check_req() {
    t.ok(confirm_req, 'The confirm_request event should have fired by now')
    t.ok(confirm_req.agent, 'The confirm request has an agent')

    t.ok(follow_req, 'The follow_request event should have fired by now')
    t.ok(follow_req.agent, 'The follow request has an agent')

    // Confirm that the changes follower is not still in the pool.
    var host = 'localhost:5984'
    var sockets = follow_req.req.agent.sockets[host] || []
    sockets.forEach(function(socket, i) {
      t.isNot(socket, follow_req.req.connection, 'The changes follower is not socket '+i+' in the agent pool')
    })

    feed.stop()
    t.end()
  }
})

test('Heartbeats', function(t) {
  t.ok(couch.rtt(), 'The couch RTT is known')
  var check_time = couch.rtt() * 3.5 // Enough time for 3 heartbeats.

  var beats = 0
    , retries = 0

  var feed = follow(couch.DB, function() {})
  feed.heartbeat = couch.rtt()
  feed.on('response', function() { feed.retry_delay = 1 })

  feed.on('heartbeat', function() { beats += 1 })
  feed.on('retry', function() { retries += 1 })

  feed.on('catchup', function() {
    t.equal(beats, 0, 'Still 0 heartbeats after receiving changes')
    t.equal(retries, 0, 'Still 0 retries after receiving changes')

    //console.error('Waiting ' + couch.rtt() + ' * 3 = ' + check_time + ' to check stuff')
    setTimeout(check_counters, check_time)
    function check_counters() {
      t.equal(beats, 3, 'Three heartbeats ('+couch.rtt()+') fired after '+check_time+' ms')
      t.equal(retries, 0, 'No retries after '+check_time+' ms')

      feed.stop()
      t.end()
    }
  })
})

test('Catchup events', function(t) {
  t.ok(couch.rtt(), 'The couch RTT is known')

  var feed = follow(couch.DB, function() {})
  var last_seen = 0

  feed.on('change', function(change) { last_seen = change.seq })
  feed.on('catchup', function(id) {
    t.equal(last_seen, 3, 'The catchup event fires after the change event that triggered it')
    t.equal(id       , 3, 'The catchup event fires on the seq_id of the latest change')
    feed.stop()
    t.end()
  })
})

test('Data due on a paused feed', function(t) {
  t.ok(couch.rtt(), 'The couch RTT is known')
  var HB = couch.rtt() * 5
    , HB_DUE = HB * 1.25 // This comes from the hard-coded constant in feed.js.

  var events = []
    , last_event = new Date

  function ev(type) {
    var now = new Date
    events.push({'elapsed':now - last_event, 'event':type})
    last_event = now
  }

  var feed = follow(couch.DB, function() {})
  feed.heartbeat = HB
  feed.on('response', function() { feed.retry_delay = 1 })
  // TODO
  // feed.pause()

  feed.on('heartbeat', function() { ev('heartbeat') })
  feed.on('change'   , function() { ev('change')    })
  feed.on('timeout'  , function() { ev('timeout')   })
  feed.on('retry'    , function() { ev('retry')     })
  feed.on('pause'    , function() { ev('pause')     })
  feed.on('resume'   , function() { ev('resume')    })
  feed.on('stop'     , function() { ev('stop')      })

  feed.on('change', function(change) {
    if(change.seq == 1) {
      feed.pause()
      // Stay paused long enough for three heartbeats to be overdue.
      setTimeout(function() { feed.resume() }, HB_DUE * 3 * 1.1)
    }

    if(change.seq == 3) {
      feed.stop()
      setTimeout(check_results, couch.rtt())
    }
  })

  function check_results() {
    console.error('rtt=%j HB=%j', couch.rtt(), HB)
    events.forEach(function(event, i) { console.error('Event %d: %j', i, event) })

    t.equal(events[0].event, 'change', 'First event was a change')
    t.ok(events[0].elapsed < couch.rtt(), 'First change came quickly')

    t.equal(events[1].event, 'pause', 'Second event was a pause, reacting to the first change')
    t.ok(events[1].elapsed < 10, 'Pause called/fired immediately after the change')

    t.equal(events[2].event, 'timeout', 'Third event was the first timeout')
    t.ok(percent(events[2].elapsed, HB_DUE) > 95, 'First timeout fired when the heartbeat was due')

    t.equal(events[3].event, 'retry', 'Fourth event is a retry')
    t.ok(events[3].elapsed < 10, 'First retry fires immediately after the first timeout')

    t.equal(events[4].event, 'timeout', 'Fifth event was the second timeout')
    t.ok(percent(events[4].elapsed, HB_DUE) > 95, 'Second timeout fired when the heartbeat was due')

    t.equal(events[5].event, 'retry', 'Sixth event is a retry')
    t.ok(events[5].elapsed < 10, 'Second retry fires immediately after the second timeout')

    t.equal(events[6].event, 'timeout', 'Seventh event was the third timeout')
    t.ok(percent(events[6].elapsed, HB_DUE) > 95, 'Third timeout fired when the heartbeat was due')

    t.equal(events[7].event, 'retry', 'Eighth event is a retry')
    t.ok(events[7].elapsed < 10, 'Third retry fires immediately after the third timeout')

    t.equal(events[8].event, 'resume', 'Ninth event resumed the feed')

    t.equal(events[9].event, 'change', 'Tenth event was the second change')
    t.ok(events[9].elapsed < 10, 'Second change came immediately after resume')

    t.equal(events[10].event, 'change', 'Eleventh event was the third change')
    t.ok(events[10].elapsed < 10, 'Third change came immediately after the second change')

    t.equal(events[11].event, 'stop', 'Twelfth event was the feed stopping')
    t.ok(events[11].elapsed < 10, 'Stop event came immediately in response to the third change')

    t.notOk(events[12], 'No thirteenth event')

    return t.end()
  }

  function percent(a, b) {
    var num = Math.min(a, b)
      , denom = Math.max(a, b)
    return 100 * num / denom
  }
})

test('Events for DB confirmation and hitting the original seq', function(t) {
  t.plan(7)
  var feed = follow(couch.DB, on_change)

  var events = { 'confirm':null }
  feed.on('confirm', function(db) { events.confirm = db })
  feed.on('catchup', caught_up)

  // This will run 3 times.
  function on_change(er, ch) {
    t.false(er, 'No problem with the feed')
    if(ch.seq == 3) {
      t.ok(events.confirm, 'Confirm event fired')
      t.equal(events.confirm && events.confirm.db_name, 'follow_test', 'Confirm event returned the Couch DB object')
      t.equal(events.confirm && events.confirm.update_seq, 3, 'Confirm event got the update_seq right')
    }
  }

  function caught_up(seq) {
    t.equal(seq, 3, 'Catchup event fired on update 3')

    feed.stop()
    t.end()
  }
})

test('Handle a deleted database', function(t) {
  var feed = follow(couch.DB, function(er, change) {
    if(er)
      return t.equal(er.last_seq, 3, 'Got an error for the deletion event')

    if(change.seq < 3)
      return

    t.equal(change.seq, 3, 'Got change number 3')

    var redo_er
    couch.redo(function(er) { redo_er = er })

    setTimeout(check_results, couch.rtt() * 2)
    function check_results() {
      t.false(er, 'No problem redoing the couch')
      t.end()
    }
  })
})

test('Follow _db_updates', function (t) {
  t.plan(4);
  var count = 0;
  var types = ['created'];
  var db_name = couch.DB.split('/').slice(-1)[0];
  var feed = new follow.Feed({db: couch.DB_UPDATES});
  feed.on('error', function (error) {
    t.false(error, 'Error in feed ' + error);
  });

  feed.on('change', function (change) {
    t.ok(change, 'Received a change ' + JSON.stringify(change));
    t.equal(change.type, types[count], 'Change should be of type "' + types[count] + '"');
    t.equal(change.db_name, db_name + 1, 'Change should have db_name with the name of the db where the change occoured.');
    feed.stop();
  });

  feed.start();
  couch.create_and_delete_db(t, function () {
    t.ok(true, 'things happened');
  });
});
