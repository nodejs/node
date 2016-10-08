var tap = require('tap')
  , test = tap.test
  , util = require('util')
  , request = require('request')

var couch = require('./couch')
  , follow = require('../api')


couch.setup(test)

test('The Changes stream API', function(t) {
  var feed = new follow.Changes

  t.type(feed.statusCode, 'null', 'Changes has a .statusCode (initially null)')
  t.type(feed.setHeader, 'function', 'Changes has a .setHeader() method')
  t.type(feed.headers, 'object', 'Changes has a .headers object')
  t.same(feed.headers, {}, 'Changes headers are initially empty')

  t.end()
})

test('Readable Stream API', function(t) {
  var feed = new follow.Changes

  t.is(feed.readable, true, 'Changes is a readable stream')

  t.type(feed.setEncoding, 'function', 'Changes has .setEncoding() method')
  t.type(feed.pause, 'function', 'Changes has .pause() method')
  t.type(feed.resume, 'function', 'Changes has .resume() method')
  t.type(feed.destroy, 'function', 'Changes has .destroy() method')
  t.type(feed.destroySoon, 'function', 'Changes has .destroySoon() method')
  t.type(feed.pipe, 'function', 'Changes has .pipe() method')

  t.end()
})

test('Writatable Stream API', function(t) {
  var feed = new follow.Changes

  t.is(feed.writable, true, 'Changes is a writable stream')

  t.type(feed.write, 'function', 'Changes has .write() method')
  t.type(feed.end, 'function', 'Changes has .end() method')
  t.type(feed.destroy, 'function', 'Changes has .destroy() method')
  t.type(feed.destroySoon, 'function', 'Changes has .destroySoon() method')

  t.end()
})

test('Error conditions', function(t) {
  var feed = new follow.Changes
  t.throws(write, 'Throw if the feed type is not defined')

  feed = new follow.Changes
  feed.feed = 'neither longpoll nor continuous'
  t.throws(write, 'Throw if the feed type is not longpoll nor continuous')

  feed = new follow.Changes({'feed':'longpoll'})
  t.throws(write('stuff'), 'Throw if the "results" line is not sent first')

  feed = new follow.Changes({'feed':'longpoll'})
  t.doesNotThrow(write('')    , 'Empty string is fine waiting for "results"')
  t.doesNotThrow(write('{')   , 'This could be the "results" line')
  t.doesNotThrow(write('"resu', 'Another part of the "results" line'))
  t.doesNotThrow(write('')    , 'Another empty string is still fine')
  t.doesNotThrow(write('lts":', 'Final part of "results" line still good'))
  t.throws(write(']'), 'First line was not {"results":[')

  feed = new follow.Changes
  feed.feed = 'continuous'
  t.doesNotThrow(write(''), 'Empty string is fine for a continuous feed')
  t.throws(end('{"results":[\n'), 'Continuous stream does not want a header')

  feed = new follow.Changes({'feed':'continuous'})
  t.throws(write('hi\n'), 'Continuous stream wants objects')

  feed = new follow.Changes({'feed':'continuous'})
  t.throws(end('[]\n'), 'Continuous stream wants "real" objects, not Array')

  feed = new follow.Changes({'feed':'continuous'})
  t.throws(write('{"seq":1,"id":"hi","changes":[{"rev":"1-869df2efe56ff5228e613ceb4d561b35"}]},\n'),
           'Continuous stream does not want a comma')

  var types = ['longpoll', 'continuous']
  types.forEach(function(type) {
    var bad_writes = [ {}, null, ['a string (array)'], {'an':'object'}]
    bad_writes.forEach(function(obj) {
      feed = new follow.Changes
      feed.feed = type

      t.throws(write(obj), 'Throw for bad write to '+type+': ' + util.inspect(obj))
    })

    feed = new follow.Changes
    feed.feed = type

    var valid = (type == 'longpoll')
                  ? '{"results":[\n{}\n],\n"last_seq":1}'
                  : '{"seq":1,"id":"doc"}'

    t.throws(buf(valid, 'but_invalid_encoding'), 'Throw for buffer with bad encoding')
  })

  t.end()

  function buf(data, encoding) {
    return write(new Buffer(data), encoding)
  }

  function write(data, encoding) {
    if(data === undefined)
      return feed.write('blah')
    return function() { feed.write(data, encoding) }
  }

  function end(data, encoding) {
    return function() { feed.end(data, encoding) }
  }
})

test('Longpoll feed', function(t) {
  for(var i = 0; i < 2; i++) {
    var feed = new follow.Changes({'feed':'longpoll'})

    var data = []
    feed.on('data', function(d) { data.push(d) })

    function encode(data) { return (i == 0) ? data : new Buffer(data) }
    function write(data) { return function() { feed.write(encode(data)) } }
    function end(data) { return function() { feed.end(encode(data)) } }

    t.doesNotThrow(write('{"results":[')           , 'Longpoll header')
    t.doesNotThrow(write('{}')                     , 'Empty object')
    t.doesNotThrow(write(',{"foo":"bar"},')        , 'Comma prefix and suffix')
    t.doesNotThrow(write('{"two":"bar"},')         , 'Comma suffix')
    t.doesNotThrow(write('{"three":3},{"four":4}'), 'Two objects on one line')
    t.doesNotThrow(end('],\n"last_seq":3}\n')      , 'Longpoll footer')

    t.equal(data.length, 5, 'Five data events fired')
    t.equal(data[0], '{}', 'First object emitted')
    t.equal(data[1], '{"foo":"bar"}', 'Second object emitted')
    t.equal(data[2], '{"two":"bar"}', 'Third object emitted')
    t.equal(data[3], '{"three":3}', 'Fourth object emitted')
    t.equal(data[4], '{"four":4}', 'Fifth object emitted')
  }

  t.end()
})

test('Longpoll pause', function(t) {
  var feed = new follow.Changes({'feed':'longpoll'})
    , all = {'results':[{'change':1}, {'second':'change'},{'change':'#3'}], 'last_seq':3}
    , start = new Date

  var events = []

  feed.on('data', function(change) {
    change = JSON.parse(change)
    change.elapsed = new Date - start
    events.push(change)
  })

  feed.once('data', function(data) {
    t.equal(data, '{"change":1}', 'First data event was the first change')
    feed.pause()
    setTimeout(function() { feed.resume() }, 100)
  })

  feed.on('end', function() {
    t.equal(feed.readable, false, 'Feed is no longer readable')
    events.push('END')
  })

  setTimeout(check_events, 150)
  feed.end(JSON.stringify(all))

  function check_events() {
    t.equal(events.length, 3+1, 'Three data events, plus the end event')

    t.ok(events[0].elapsed < 10, 'Immediate emit first data event')
    t.ok(events[1].elapsed >= 100 && events[1].elapsed < 125, 'About 100ms delay until the second event')
    t.ok(events[2].elapsed - events[1].elapsed < 10, 'Immediate emit of subsequent event after resume')
    t.equal(events[3], 'END', 'End event was fired')

    t.end()
  }
})

test('Continuous feed', function(t) {
  for(var i = 0; i < 2; i++) {
    var feed = new follow.Changes({'feed':'continuous'})

    var data = []
    feed.on('data', function(d) { data.push(d) })
    feed.on('end', function() { data.push('END') })

    var beats = 0
    feed.on('heartbeat', function() { beats += 1 })

    function encode(data) { return (i == 0) ? data : new Buffer(data) }
    function write(data) { return function() { feed.write(encode(data)) } }
    function end(data) { return function() { feed.end(encode(data)) } }

    // This also tests whether the feed is compacting or tightening up the JSON.
    t.doesNotThrow(write('{    }\n')                   , 'Empty object')
    t.doesNotThrow(write('\n')                         , 'Heartbeat')
    t.doesNotThrow(write('{ "foo" : "bar" }\n')        , 'One object')
    t.doesNotThrow(write('{"three":3}\n{ "four": 4}\n'), 'Two objects sent in one chunk')
    t.doesNotThrow(write('')                           , 'Empty string')
    t.doesNotThrow(write('\n')                         , 'Another heartbeat')
    t.doesNotThrow(write('')                           , 'Another empty string')
    t.doesNotThrow(write('{   "end"  ')                , 'Partial object 1/4')
    t.doesNotThrow(write(':')                          , 'Partial object 2/4')
    t.doesNotThrow(write('tru')                        , 'Partial object 3/4')
    t.doesNotThrow(end('e}\n')                         , 'Partial object 4/4')

    t.equal(data.length, 6, 'Five objects emitted, plus the end event')
    t.equal(beats, 2, 'Two heartbeats emitted')

    t.equal(data[0], '{}', 'First object emitted')
    t.equal(data[1], '{"foo":"bar"}', 'Second object emitted')
    t.equal(data[2], '{"three":3}', 'Third object emitted')
    t.equal(data[3], '{"four":4}', 'Fourth object emitted')
    t.equal(data[4], '{"end":true}', 'Fifth object emitted')
    t.equal(data[5], 'END', 'End event fired')
  }

  t.end()
})

test('Continuous pause', function(t) {
  var feed = new follow.Changes({'feed':'continuous'})
    , all = [{'change':1}, {'second':'change'},{'#3':'change'}]
    , start = new Date

  var events = []

  feed.on('end', function() {
    t.equal(feed.readable, false, 'Feed is not readable after "end" event')
    events.push('END')
  })

  feed.on('data', function(change) {
    change = JSON.parse(change)
    change.elapsed = new Date - start
    events.push(change)
  })

  feed.once('data', function(data) {
    t.equal(data, '{"change":1}', 'First data event was the first change')
    t.equal(feed.readable, true, 'Feed is readable after first data event')
    feed.pause()
    t.equal(feed.readable, true, 'Feed is readable after pause()')

    setTimeout(unpause, 100)
    function unpause() {
      t.equal(feed.readable, true, 'Feed is readable just before resume()')
      feed.resume()
    }
  })

  setTimeout(check_events, 150)
  all.forEach(function(obj) {
    feed.write(JSON.stringify(obj))
    feed.write("\r\n")
  })
  feed.end()

  function check_events() {
    t.equal(events.length, 3+1, 'Three data events, plus the end event')

    t.ok(events[0].elapsed < 10, 'Immediate emit first data event')
    t.ok(events[1].elapsed >= 100 && events[1].elapsed < 125, 'About 100ms delay until the second event')
    t.ok(events[2].elapsed - events[1].elapsed < 10, 'Immediate emit of subsequent event after resume')
    t.equal(events[3], 'END', 'End event was fired')

    t.end()
  }
})

test('Paused while heartbeats are arriving', function(t) {
  var feed = new follow.Changes({'feed':'continuous'})
    , start = new Date

  var events = []

  feed.on('data', function(change) {
    change = JSON.parse(change)
    change.elapsed = new Date - start
    events.push(change)
  })

  feed.on('heartbeat', function(hb) {
    var hb = {'heartbeat':true, 'elapsed':(new Date) - start}
    events.push(hb)
  })

  feed.once('data', function(data) {
    t.equal(data, '{"change":1}', 'First data event was the first change')
    feed.pause()
    t.equal(feed.readable, true, 'Feed is readable after pause()')

    setTimeout(unpause, 350)
    function unpause() {
      t.equal(feed.readable, true, 'Feed is readable just before resume()')
      feed.resume()
    }
  })

  // Simulate a change, then a couple heartbeats every 100ms, then more changes.
  var writes = []
  function write(data) {
    var result = feed.write(data)
    writes.push(result)
  }

  setTimeout(function() { write('{"change":1}\n') }, 0)
  setTimeout(function() { write('\n')             }, 100)
  setTimeout(function() { write('\n')             }, 200)
  setTimeout(function() { write('\n')             }, 300)
  setTimeout(function() { write('\n')             }, 400)
  setTimeout(function() { write('{"change":2}\n') }, 415)
  setTimeout(function() { write('{"change":3}\n') }, 430)

  setTimeout(check_events, 500)
  function check_events() {
    t.ok(events[0].change   , 'First event was data (a change)')
    t.ok(events[1].heartbeat, 'Second event was a heartbeat')
    t.ok(events[2].heartbeat, 'Third event was a heartbeat')
    t.ok(events[3].heartbeat, 'Fourth event was a heartbeat')
    t.ok(events[4].heartbeat, 'Fifth event was a heartbeat')
    t.ok(events[5].change   , 'Sixth event was data')
    t.ok(events[6].change   , 'Seventh event was data')

    t.ok(events[0].elapsed < 10, 'Immediate emit first data event')
    t.ok(events[1].elapsed > 350, 'First heartbeat comes after the pause')
    t.ok(events[2].elapsed > 350, 'Second heartbeat comes after the pause')
    t.ok(events[3].elapsed > 350, 'Third heartbeat comes after the pause')
    t.ok(events[4].elapsed > 350, 'Fourth heartbeat comes after the pause')

    t.ok(events[3].elapsed < 360, 'Third heartbeat came right after the resume')
    t.ok(events[4].elapsed >= 400, 'Fourth heartbeat came at the normal time')

    t.equal(writes[0], true , 'First write flushed')
    t.equal(writes[1], false, 'Second write (first heartbeat) did not flush because the feed was paused')
    t.equal(writes[2], false, 'Third write did not flush due to pause')
    t.equal(writes[3], false, 'Fourth write did not flush due to pause')
    t.equal(writes[4], true , 'Fifth write (fourth heartbeat) flushed due to resume()')
    t.equal(writes[5], true , 'Sixth write flushed normally')
    t.equal(writes[6], true , 'Seventh write flushed normally')

    feed.end()
    t.end()
  }
})

test('Feeds from couch', function(t) {
  t.ok(couch.rtt(), 'RTT to couch is known')

  var did = 0
  function done() {
    did += 1
    if(did == 2)
      t.end()
  }

  var types = [ 'longpoll', 'continuous' ]
  types.forEach(function(type) {
    var feed = new follow.Changes({'feed':type})
    setTimeout(check_changes, couch.rtt() * 2)

    var events = []
    feed.on('data', function(data) { events.push(JSON.parse(data)) })
    feed.on('end', function() { events.push('END') })

    var uri = couch.DB + '/_changes?feed=' + type
    var req = request({'uri':uri})

    // Compatibility with the old onResponse option.
    req.on('response', function(res) { on_response(null, res, res.body) })

    // Disconnect the continuous feed after a while.
    if(type == 'continuous')
      setTimeout(function() { feed.destroy() }, couch.rtt() * 1)

    function on_response(er, res, body) {
      t.false(er, 'No problem fetching '+type+' feed: ' + uri)
      t.type(body, 'undefined', 'No data in '+type+' callback. This is an onResponse callback')
      t.type(res.body, 'undefined', 'No response body in '+type+' callback. This is an onResponse callback')
      t.ok(req.response, 'The request object has its '+type+' response by now')

      req.pipe(feed)

      t.equal(feed.statusCode, 200, 'Upon piping from request, the statusCode is set')
      t.ok('content-type' in feed.headers, 'Upon piping from request, feed has headers set')
    }

    function check_changes() {
      var expected_count = 3
      if(type == 'longpoll')
        expected_count += 1 // For the "end" event

      t.equal(events.length, expected_count, 'Change event count for ' + type)

      t.equal(events[0].seq, 1, 'First '+type+' update sequence id')
      t.equal(events[1].seq, 2, 'Second '+type+' update sequence id')
      t.equal(events[2].seq, 3, 'Third '+type+' update sequence id')

      t.equal(good_id(events[0]), true, 'First '+type+' update is a good doc id: ' + events[0].id)
      t.equal(good_id(events[1]), true, 'Second '+type+' update is a good doc id: ' + events[1].id)
      t.equal(good_id(events[2]), true, 'Third '+type+' update is a good doc id: ' + events[2].id)

      if(type == 'longpoll')
        t.equal(events[3], 'END', 'End event fired for '+type)
      else
        t.type(events[3], 'undefined', 'No end event for a destroyed continuous feed')

      done()
    }

    var good_ids = {'doc_first':true, 'doc_second':true, 'doc_third':true}
    function good_id(event) {
      var is_good = good_ids[event.id]
      delete good_ids[event.id]
      return is_good
    }
  })
})

test('Pausing and destroying a feed mid-stream', function(t) {
  t.ok(couch.rtt(), 'RTT to couch is known')
  var IMMEDIATE = 20
    , FIRST_PAUSE = couch.rtt() * 8
    , SECOND_PAUSE = couch.rtt() * 12

  // To be really, really sure that backpressure goes all the way to couch, create more
  // documents than could possibly be buffered. Linux and OSX seem to have a 16k MTU for
  // the local interface, so a few hundred kb worth of document data should cover it.
  couch.make_data(512 * 1024, check)

  var types = ['longpoll', 'continuous']
  function check(bulk_docs_count) {
    var type = types.shift()
    if(!type)
      return t.end()

    var feed = new follow.Changes
    feed.feed = type

    var destroys = 0
    function destroy() {
      destroys += 1
      feed.destroy()

      // Give one more RTT time for everything to wind down before checking how it all went.
      if(destroys == 1)
        setTimeout(check_events, couch.rtt())
    }

    var events = {'feed':[], 'http':[], 'request':[]}
      , firsts = {'feed':null, 'http':null, 'request':null}

    function ev(type, value) {
      var now = new Date
      firsts[type] = firsts[type] || now
      events[type].push({'elapsed':now - firsts[type], 'at':now, 'val':value, 'before_destroy':(destroys == 0)})
    }

    feed.on('heartbeat', function() { ev('feed', 'heartbeat') })
    feed.on('error', function(er) { ev('feed', er) })
    feed.on('close', function() { ev('feed', 'close') })
    feed.on('data', function(data) { ev('feed', data) })
    feed.on('end', function() { ev('feed', 'end') })

    var data_count = 0
    feed.on('data', function() {
      data_count += 1
      if(data_count == 4) {
        feed.pause()
        setTimeout(function() { feed.resume() }, FIRST_PAUSE)
      }

      if(data_count == 7) {
        feed.pause()
        setTimeout(function() { feed.resume() }, SECOND_PAUSE - FIRST_PAUSE)
      }

      if(data_count >= 10)
        destroy()
    })

    var uri = couch.DB + '/_changes?include_docs=true&feed=' + type
    if(type == 'continuous')
      uri += '&heartbeat=' + Math.floor(couch.rtt())

    var req = request({'uri':uri})
    req.on('response', function(res) { feed_response(null, res, res.body) })
    req.on('error', function(er) { ev('request', er) })
    req.on('close', function() { ev('request', 'close') })
    req.on('data', function(d) { ev('request', d) })
    req.on('end', function() { ev('request', 'end') })
    req.pipe(feed)

    function feed_response(er, res) {
      if(er) throw er

      res.on('error', function(er) { ev('http', er) })
      res.on('close', function() { ev('http', 'close') })
      res.on('data', function(d) { ev('http', d) })
      res.on('end', function() { ev('http', 'end') })

      t.equal(events.feed.length, 0, 'No feed events yet: ' + type)
      t.equal(events.http.length, 0, 'No http events yet: ' + type)
      t.equal(events.request.length, 0, 'No request events yet: ' + type)
    }

    function check_events() {
      t.equal(destroys, 1, 'Only necessary to call destroy once: ' + type)
      t.equal(events.feed.length, 10, 'Ten '+type+' data events fired')
      if(events.feed.length != 10)
        events.feed.forEach(function(e, i) {
          console.error((i+1) + ') ' + util.inspect(e))
        })

      events.feed.forEach(function(event, i) {
        var label = type + ' event #' + (i+1) + ' at ' + event.elapsed + ' ms'

        t.type(event.val, 'string', label+' was a data string')
        t.equal(event.before_destroy, true, label+' fired before the destroy')

        var change = null
        t.doesNotThrow(function() { change = JSON.parse(event.val) }, label+' was JSON: ' + type)
        t.ok(change && change.seq > 0 && change.id, label+' was change data: ' + type)

        // The first batch of events should have fired quickly (IMMEDIATE), then silence. Then another batch
        // of events at the FIRST_PAUSE mark. Then more silence. Then a final batch at the SECOND_PAUSE mark.
        if(i < 4)
          t.ok(event.elapsed < IMMEDIATE, label+' was immediate (within '+IMMEDIATE+' ms)')
        else if(i < 7)
          t.ok(is_almost(event.elapsed, FIRST_PAUSE), label+' was after the first pause (about '+FIRST_PAUSE+' ms)')
        else
          t.ok(is_almost(event.elapsed, SECOND_PAUSE), label+' was after the second pause (about '+SECOND_PAUSE+' ms)')
      })

      if(type == 'continuous') {
        t.ok(events.http.length >= 6, 'Should have at least seven '+type+' HTTP events')
        t.ok(events.request.length >= 6, 'Should have at least seven '+type+' request events')

        t.ok(events.http.length < 200, type+' HTTP events ('+events.http.length+') stop before 200')
        t.ok(events.request.length < 200, type+' request events ('+events.request.length+') stop before 200')

        var frac = events.http.length / bulk_docs_count
        t.ok(frac < 0.10, 'Percent of http events received ('+frac.toFixed(1)+'%) is less than 10% of the data')

        frac = events.request.length / bulk_docs_count
        t.ok(frac < 0.10, type+' request events received ('+frac.toFixed(1)+'%) is less than 10% of the data')
      }

      return check(bulk_docs_count)
    }
  }
})

//
// Utilities
//

function is_almost(actual, expected) {
  var tolerance = 0.10 // 10%
    , diff = Math.abs(actual - expected)
    , fraction = diff / expected
  return fraction < tolerance
}
