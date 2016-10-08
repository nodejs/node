// CouchDB tests
//
// This module is also a library for other test modules.

var tap = require('tap')
  , util = require('util')
  , assert = require('assert')
  , request = require('request')

var follow = require('../api')
  , DB = process.env.db || 'http://localhost:5984/follow_test'
  , DB_UPDATES = process.env.db_updates || 'http://localhost:5984/_db_updates'
  , RTT = null


module.exports = { 'DB': DB
                 , 'DB_UPDATES': DB_UPDATES
                 , 'rtt' : get_rtt
                 , 'redo': redo_couch
                 , 'setup': setup_test
                 , 'make_data': make_data
                 , 'create_and_delete_db': create_and_delete_db
                 }


function get_rtt() {
  if(!RTT)
    throw new Error('RTT was not set. Use setup(test) or redo(callback)')
  return RTT
}


// Basically a redo but testing along the way.
function setup_test(test_func) {
  assert.equal(typeof test_func, 'function', 'Please provide tap.test function')

  test_func('Initialize CouchDB', function(t) {
    init_db(t, function(er, rtt) {
      RTT = rtt
      t.end()
    })
  })
}

function redo_couch(callback) {
  function noop() {}
  var t = { 'ok':noop, 'false':noop, 'equal':noop, 'end':noop }
  init_db(t, function(er, rtt) {
    if(rtt)
      RTT = rtt
    return callback(er)
  })
}

function init_db(t, callback) {
  var create_begin = new Date

  request.del({uri:DB, json:true}, function(er, res) {
    t.false(er, 'Clear old test DB: ' + DB)
    t.ok(!res.body.error || res.body.error == 'not_found', 'Couch cleared old test DB: ' + DB)

    request.put({uri:DB, json:true}, function(er, res) {
      t.false(er, 'Create new test DB: ' + DB)
      t.false(res.body.error, 'Couch created new test DB: ' + DB)

      var values = ['first', 'second', 'third']
        , stores = 0
      values.forEach(function(val) {
        var doc = { _id:'doc_'+val, value:val }

        request.post({uri:DB, json:doc}, function(er, res) {
          t.false(er, 'POST document')
          t.equal(res.statusCode, 201, 'Couch stored test document')

          stores += 1
          if(stores == values.length) {
            var rtt = (new Date) - create_begin
            callback(null, rtt)
            //request.post({uri:DB, json:{_id:'_local/rtt', ms:(new Date)-begin}}, function(er, res) {
            //  t.false(er, 'Store RTT value')
            //  t.equal(res.statusCode, 201, 'Couch stored RTT value')
            //  t.end()
            //})
          }
        })
      })
    })
  })
}

function create_and_delete_db(t, callback) {
  request.put({ uri: DB + 1, json: true}, function (er, res) {
    t.false(er, 'create test db');
    request.del({uri: DB +1, json: true}, function (er, res) {
      t.false(er, 'Clear old test DB: ' + DB)
      t.ok(!res.body.error);
      callback();
    });
  });
}


function make_data(minimum_size, callback) {
  var payload = {'docs':[]}
    , size = 0

  // TODO: Make document number 20 really large, at least over 9kb.
  while(size < minimum_size) {
    var doc = {}
      , key_count = rndint(0, 25)

    while(key_count-- > 0)
      doc[rndstr(8)] = rndstr(20)

    // The 20th document has one really large string value.
    if(payload.docs.length == 19) {
      var big_str = rndstr(9000, 15000)
      doc.big = {'length':big_str.length, 'value':big_str}
    }

    size += JSON.stringify(doc).length // This is an underestimate because an _id and _rev will be added.
    payload.docs.push(doc)
  }

  request.post({'uri':DB+'/_bulk_docs', 'json':payload}, function(er, res, body) {
    if(er) throw er

    if(res.statusCode != 201)
      throw new Error('Bad bulk_docs update: ' + util.inspect(res.body))

    if(res.body.length != payload.docs.length)
      throw new Error('Should have results for '+payload.docs.length+' doc insertions')

    body.forEach(function(result) {
      if(!result || !result.id || !result.rev)
        throw new Error('Bad bulk_docs response: ' + util.inspect(result))
    })

    return callback(payload.docs.length)
  })

  function rndstr(minlen, maxlen) {
    if(!maxlen) {
      maxlen = minlen
      minlen = 1
    }

    var str = ""
      , length = rndint(minlen, maxlen)

    while(length-- > 0)
      str += String.fromCharCode(rndint(97, 122))

    return str
  }

  function rndint(min, max) {
    return min + Math.floor(Math.random() * (max - min + 1))
  }
}


if(require.main === module)
  setup_test(tap.test)
